#include <stdio.h>   // 标准输入输出库
#include <string.h>   // 字符串处理库
#include <unistd.h>   // POSIX标准库函数
#include "app_init.h"  // 应用初始化头文件
#include "cmsis_os2.h"  // CMSIS-RTOS2 API头文件
#include "common_def.h"  // 通用定义头文件
#include "soc_osal.h"  // SOC操作系统抽象层头文件
#include "sle_device_discovery.h"  // SLE设备发现相关头文件
#include "sle_uuid_client.h"  // SLE客户端相关头文件
#include "sle_uuid_server.h"  // SLE服务端相关头文件
#include "sle_server_adv.h"   // SLE广播相关头文件
#include "osal_addr.h"        // For random_mac_addr
#include "sle_hybrid.h"       // 混合模式相关头文件
#include "gpio.h"       // SLE通用定义头文件
#include "pinctrl.h"  // 引脚控制头文件


// ================== 节点状态管理 ==================

// 网络状态信息
typedef struct {
    node_role_t role;
    uint8_t level;
    uint16_t parent_conn_id;
} node_network_status_t;

static node_network_status_t g_network_status;
static bool g_is_reverting = false;
static sle_addr_t g_local_addr = {0}; // 统一的设备地址

// 外部函数声明，用于注册SLE通用回调函数
extern errcode_t sle_register_common_cbks(void);

// 新增函数，供其他模块获取统一地址
sle_addr_t* hybrid_get_local_addr(void)
{
    return &g_local_addr;
}

node_role_t hybrid_node_get_role(void)
{
    return g_network_status.role;
}

uint8_t hybrid_node_get_level(void)
{
    return g_network_status.level;
}

uint16_t hybrid_node_get_parent_conn_id(void)
{
    return g_network_status.parent_conn_id;
}

bool hybrid_node_is_reverting_to_orphan(void)
{
    return g_is_reverting;
}

/**
 * @brief 节点初始化为孤儿
 */
void hybrid_node_init(void)
{
    g_network_status.role = NODE_ROLE_ORPHAN;
    g_network_status.level = 0; // 孤儿节点的初始层级设为0
    g_network_status.parent_conn_id = 0;
    osal_printk("Node initialized as ORPHAN, level %d\r\n", g_network_status.level);
    
    // 作为孤儿，只需要启动服务端功能并开始广播
    sle_hybrids_init();
    sle_hybridc_init();
    uapi_pin_set_mode(2, PIN_MODE_0);
    uapi_gpio_set_dir(2, GPIO_DIRECTION_OUTPUT);
    sle_update_adv_data();
}

/**
 * @brief 节点转变为成员
 * @param parent_conn_id 父节点的连接ID
 * @param parent_level 父节点的网络层级
 */
void hybrid_node_become_member(uint16_t parent_conn_id, uint8_t parent_level)
{
    if (g_network_status.role == NODE_ROLE_MEMBER) {
        return; // 已经是成员，无需转换
    }
    g_network_status.role = NODE_ROLE_MEMBER;
    g_network_status.level = parent_level + 1;
    g_network_status.parent_conn_id = parent_conn_id;
    osal_printk("Node becomes MEMBER, level %d, parent_conn_id %d\r\n", g_network_status.level, parent_conn_id);

    uapi_gpio_set_val(2, GPIO_LEVEL_HIGH); // 设置GPIO引脚高电平，表示已连接
    // 作为成员，需要启动客户端功能去寻找自己的子节点

    sle_start_scan();

    // 成为成员后，节点应停止广播。
    // 在被连接时（作为孤儿）广播已经停止，此处无需再操作。
}

/**
 * @brief 节点恢复为孤儿
 */
void hybrid_node_revert_to_orphan(void)
{
    g_is_reverting = true;
    osal_printk("Node reverting to ORPHAN from level %d\r\n", g_network_status.level);
    // 断开所有子节点的连接
    sle_hybridc_disconnect_all_children();

    uapi_gpio_set_val(2, GPIO_LEVEL_LOW);


    // 停止客户端的扫描
    sle_stop_seek();

    // 重置网络状态
    g_network_status.role = NODE_ROLE_ORPHAN;
    g_network_status.level = 0;
    g_network_status.parent_conn_id = 0;

    // 重新开始作为孤儿进行广播
    sle_update_adv_data();
    g_is_reverting = false;
}

/**
 * @brief SLE混合模式主任务函数
 * @param arg 传入参数，未使用
 * @note 依次初始化服务端、客户端，注册回调，启动发送测试
 */
void sle_hybrid_task(char *arg)
{
    unused(arg);  // 处理未使用的参数
    errcode_t ret = 0;  // 操作返回状态码
    
    static uint32_t report_counter = 0;
    
    // 1. 设置并统一节点的MAC地址
    sle_addr_t local_address;
    local_address.type = SLE_ADDRESS_TYPE_PUBLIC;
    random_mac_addr(local_address.addr); // 生成随机MAC
    // local_address.addr[0]=0x11;
    // local_address.addr[1]=0x22;
    // local_address.addr[2]=0x33;
    // local_address.addr[3]=0x44;
    // local_address.addr[4]=0x55;
    // local_address.addr[5]=0x01;
    (void)memcpy_s(g_local_addr.addr, SLE_ADDR_LEN, local_address.addr, SLE_ADDR_LEN);
    g_local_addr.type = local_address.type;
    sle_set_local_addr(&g_local_addr);
    
    osal_printk("Node MAC address: %02x:%02x:%02x:%02x:%02x:%02x\r\n",
           g_local_addr.addr[0], g_local_addr.addr[1], g_local_addr.addr[2], 
           g_local_addr.addr[3], g_local_addr.addr[4], g_local_addr.addr[5]);

    // 2. 注册SLE通用回调函数
    sle_register_common_cbks();

    // 3. 启用SLE服务
    ret = enable_sle();
    if (ret != 0)
    {
        osal_printk("enable_sle fail :%x\r\n", ret);
        return;  // 启用失败直接返回
    }
    osal_printk("enable_sle succ\r\n");

    // 5. 初始化节点为孤儿状态
    hybrid_node_init();
    
    // 7. 主循环，可以根据节点状态执行不同的逻辑
    while (1) {
        // 例如，可以周期性地打印当前网络状态
        osal_msleep(5000);
        osal_printk("Current role: %d, level: %d, parent_id: %d, children: %d\r\n",
                    g_network_status.role,
                    g_network_status.level,
                    g_network_status.parent_conn_id,
                    get_active_children_count());
                     // 作为成员节点，且有父节点连接时，上报自己的数据
                     
        if (g_network_status.role == NODE_ROLE_MEMBER && sle_hybrids_is_client_connected()) {
            report_data_t data_to_report;
            data_to_report.data = report_counter++;
            (void)memcpy_s(data_to_report.origin_mac, SLE_ADDR_LEN, g_local_addr.addr, SLE_ADDR_LEN);

            osal_printk("Member node sending its own data, count: %u\r\n", data_to_report.data);
            sle_hybrids_send_data((uint8_t*)&data_to_report, sizeof(data_to_report));
        }
    }
}

// 任务优先级和栈大小定义
#define SLE_HYBRIDTASK_PRIO 24          // 混合模式任务优先级
#define SLE_HYBRID_STACK_SIZE 0x2000    // 混合模式任务栈大小(8KB)

/**
 * @brief SLE混合模式任务入口函数
 * @note 创建混合模式任务线程
 */
static void sle_hybrid_entry(void)
{
    osal_task *task_handle = NULL;
    osal_kthread_lock();
    task_handle= osal_kthread_create((osal_kthread_handler)sle_hybrid_task, 0, "sle_gatt_client",
        SLE_HYBRID_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, SLE_HYBRIDTASK_PRIO);
        osal_kfree(task_handle);
    }
    osal_kthread_unlock();
}

app_run(sle_hybrid_entry);