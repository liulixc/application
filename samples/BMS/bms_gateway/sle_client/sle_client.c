/*==============================================================================
 * 系统头文件包含区域
 * 功能：引入系统基础功能、SLE协议栈、JSON解析等必要组件
 *============================================================================*/
// 系统基础组件
#include "common_def.h"                // 通用定义，包含基础数据类型和宏定义
#include "soc_osal.h"                  // 操作系统抽象层接口，提供任务、内存、消息队列等功能
#include "securec.h"                   // 安全C库，提供安全的字符串和内存操作函数
#include "product.h"                   // 产品配置文件，包含硬件和功能配置
#include "bts_le_gap.h"                // 蓝牙低功耗GAP层接口

// SLE协议栈组件
#include "sle_device_discovery.h"      // SLE设备发现和扫描功能
#include "sle_connection_manager.h"    // SLE连接管理，处理设备连接状态
#include "sle_client.h"                // SLE客户端接口定义

// 应用层组件
#include "cJSON.h"                     // JSON解析库，用于处理BMS设备数据
#include "mqtt_demo.h"                 // MQTT通信模块，用于云端数据传输
/*==============================================================================
 * SLE协议配置参数定义区域
 * 功能：定义SLE通信、设备发现、数据传输等关键配置参数
 *============================================================================*/

#define SLE_MTU_SIZE_DEFAULT            520   // SLE连接的默认MTU大小，影响单次传输的最大数据量
#define SLE_SEEK_INTERVAL_DEFAULT       100   // SLE设备扫描间隔（毫秒），平衡发现速度和功耗
#define SLE_SEEK_WINDOW_DEFAULT         100   // SLE设备扫描窗口（毫秒），实际监听时间
#define UUID_16BIT_LEN                  2     // 16位UUID长度，用于标准蓝牙服务
#define UUID_128BIT_LEN                 16    // 128位UUID长度，用于自定义服务
#define SLE_UART_TASK_DELAY_MS          1000  // SLE任务延时间隔
#define SLE_UART_WAIT_SLE_CORE_READY_MS 5000  // 等待SLE协议栈准备就绪的时间
#define SLE_UART_RECV_CNT               1000  // 接收计数器最大值
#define SLE_UART_LOW_LATENCY_2K         2000  // 低延时模式阈值

#ifndef SLE_UART_SERVER_NAME
#define SLE_UART_SERVER_NAME            "sle_uart_server"  // SLE服务器名称标识
#endif

#define SLE_UART_CLIENT_LOG             "[sle uart client]"  // 日志前缀标识
#define SLE_UART_CLIENT_MAX_CON         8                    // 支持的最大并发连接数（8个BMS设备）
#define MAX_BMS_DEVICES 8  // 最大支持8个BMS设备

/*==============================================================================
 * 全局变量定义区域
 * 功能：定义SLE连接管理、回调函数、数据缓存等核心变量
 *============================================================================*/

// SLE协议栈相关全局变量
static ssapc_find_service_result_t g_sle_uart_find_service_result = { 0 };  // 服务发现结果存储
static sle_announce_seek_callbacks_t g_sle_uart_seek_cbk = { 0 };           // 设备扫描回调函数集
static sle_connection_callbacks_t g_sle_uart_connect_cbk = { 0 };           // 连接管理回调函数集
static ssapc_callbacks_t g_sle_uart_ssapc_cbk = { 0 };                     // SSAP客户端回调函数集
static sle_addr_t g_sle_uart_remote_addr = { 0 };                          // 远程设备地址缓存

// SLE数据传输相关变量
ssapc_write_param_t g_sle_uart_send_param = { 0 };                         // 数据发送参数配置
uint16_t g_sle_uart_conn_id[SLE_UART_CLIENT_MAX_CON] = { 0 };              // 连接ID数组，管理多设备连接
uint16_t g_sle_uart_conn_num = 0;                                          // 当前已连接设备数量

// 外部全局变量引用
extern volatile environment_msg g_env_msg[SLE_UART_CLIENT_MAX_CON];        // 全局环境消息变量，存储BMS数据


// 设备映射表
bms_device_map_t g_bms_device_map[SLE_UART_CLIENT_MAX_CON];
uint8_t g_active_device_count = 0;  // 当前活跃设备数量

/**
 * @brief 根据MAC地址获取对应的华为云平台设备ID
 * @param mac MAC地址
 * @param device_id 输出参数，存储获取到的设备ID
 * @param max_len device_id缓冲区最大长度
 * @return 0表示成功，-1表示未找到映射
 */
static int get_cloud_device_id_by_mac(const uint8_t *mac, char *device_id, size_t max_len)
{
    // 预定义的MAC地址到设备ID的映射表
    // 格式: MAC地址(6字节) -> 华为云设备ID
    static const struct {
        uint8_t mac[6];
        const char *device_id;
    } mac_to_device_id_map[] = {
        // 华为云平台预期的设备ID格式
        {{0x11, 0x22, 0x33, 0x44, 0x55, 0x01}, "680b91649314d11851158e8d_Battery01"},
        {{0x11, 0x22, 0x33, 0x44, 0x55, 0x02}, "680b91649314d11851158e8d_Battery02"},
        {{0x11, 0x22, 0x33, 0x44, 0x55, 0x03}, "680b91649314d11851158e8d_Battery03"},
        {{0x11, 0x22, 0x33, 0x44, 0x55, 0x04}, "680b91649314d11851158e8d_Battery04"},
        {{0x11, 0x22, 0x33, 0x44, 0x55, 0x05}, "680b91649314d11851158e8d_Battery05"},
        {{0x11, 0x22, 0x33, 0x44, 0x55, 0x06}, "680b91649314d11851158e8d_Battery06"},
        {{0x11, 0x22, 0x33, 0x44, 0x55, 0x07}, "680b91649314d11851158e8d_Battery07"},
        {{0x11, 0x22, 0x33, 0x44, 0x55, 0x08}, "680b91649314d11851158e8d_Battery08"},
        // 添加更多映射...
    };

    // 遍历映射表查找匹配的MAC地址
    for (size_t i = 0; i < sizeof(mac_to_device_id_map) / sizeof(mac_to_device_id_map[0]); i++) {
        if (memcmp(mac, mac_to_device_id_map[i].mac, 6) == 0) {
            // 找到匹配项，复制设备ID并返回
            strncpy(device_id, mac_to_device_id_map[i].device_id, max_len - 1);
            device_id[max_len - 1] = '\0';  // 确保字符串终止
            return 0;
        }
    }

    printf("Warning: No predefined mapping for MAC %02X:%02X:%02X:%02X:%02X:%02X\n",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], device_id);
    
    return -1;
}

/**
 * @brief 根据连接ID查找设备在数组中的索引
 * @param conn_id 连接ID
 * @return 设备索引，-1表示未找到
 */
static int find_device_index_by_conn_id(uint16_t conn_id)
{
    for (int i = 0; i < MAX_BMS_DEVICES; i++) {
        if (g_bms_device_map[i].is_active && g_bms_device_map[i].conn_id == conn_id) {
            return g_bms_device_map[i].device_index;
        }
    }
    return -1;  // 未找到
}

/**
 * @brief 为新连接的设备分配数组索引
 * @param conn_id 连接ID
 * @param device_addr 设备MAC地址
 * @return 分配的设备索引，-1表示分配失败
 */
static int allocate_device_index(uint16_t conn_id, const sle_addr_t *device_addr)
{
    if (g_active_device_count >= MAX_BMS_DEVICES) {
        printf("Maximum BMS devices reached!\r\n");
        return -1;
    }
    
    // 查找空闲的映射槽位
    for (int i = 0; i < MAX_BMS_DEVICES; i++) {
        if (!g_bms_device_map[i].is_active) {
            // 找到空闲槽位，分配给新设备
            g_bms_device_map[i].conn_id = conn_id;
            g_bms_device_map[i].device_index = i;
            g_bms_device_map[i].is_active = true;
            
            // 保存设备MAC地址用于识别
            if (device_addr != NULL) {
                memcpy(g_bms_device_map[i].device_mac, device_addr->addr, 6);
                
                // 获取对应的云平台设备ID
                get_cloud_device_id_by_mac(device_addr->addr, g_bms_device_map[i].cloud_device_id, sizeof(g_bms_device_map[i].cloud_device_id));
                printf("Mapped BMS device (MAC: %02x:%02x:%02x:%02x:%02x:%02x) to cloud device_id: %s\r\n",
                       device_addr->addr[0], device_addr->addr[1], device_addr->addr[2],
                       device_addr->addr[3], device_addr->addr[4], device_addr->addr[5],
                       g_bms_device_map[i].cloud_device_id);
            }
            
            g_active_device_count++;
            
            // 初始化对应的数据结构
            memset(&g_env_msg[i], 0, sizeof(environment_msg));
            
            printf("Allocated device index %d for conn_id %d (MAC: %02x:%02x:%02x:%02x:%02x:%02x)\r\n", 
                   i, conn_id, 
                   device_addr->addr[0], device_addr->addr[1], device_addr->addr[2],
                   device_addr->addr[3], device_addr->addr[4], device_addr->addr[5]);
            
            return i;
        }
    }
    
    return -1;  // 没有找到空闲槽位
}

/**
 * @brief 移除断开连接的设备
 * @param conn_id 断开连接的设备ID
 */
static void remove_device_by_conn_id(uint16_t conn_id)
{
    for (int i = 0; i < MAX_BMS_DEVICES; i++) {
        if (g_bms_device_map[i].is_active && g_bms_device_map[i].conn_id == conn_id) {
            // 找到对应设备，清除映射
            g_bms_device_map[i].is_active = false;
            g_bms_device_map[i].conn_id = 0;
            
            // 清除数据
            memset(&g_env_msg[i], 0, sizeof(environment_msg));
            
            g_active_device_count--;
            
            printf("Removed device at index %d, conn_id %d. Active devices: %d\r\n", 
                   i, conn_id, g_active_device_count);
            break;
        }
    }
}


/*==============================================================================
 * 消息队列数据结构定义
 * 功能：定义SLE通知数据的异步处理机制
 *============================================================================*/

/**
 * @brief SLE通知数据结构体
 * @details 用于在消息队列中传递SLE设备的通知数据
 */
typedef struct {
    uint8_t* data;          // 指向实际数据的指针
    uint16_t data_len;      // 数据长度
    uint8_t connect_id;     // 连接ID标识，用于区分不同的BMS设备
} notify_data_t;


unsigned long sle_msg_queue = 0;                  // 消息队列句柄
unsigned int sle_msg_size = sizeof(notify_data_t); // 消息大小

/*==============================================================================
 * SLE客户端接口函数
 * 功能：提供SLE数据发送参数的外部访问接口
 *============================================================================*/

/**
 * @brief 获取SLE数据发送参数的指针
 * @return ssapc_write_param_t* 指向全局发送参数结构体的指针
 * @details 为外部模块提供访问SLE数据发送参数的接口
 *          主要用于数据发送前的参数配置
 */
ssapc_write_param_t *get_g_sle_uart_send_param(void)
{
    return &g_sle_uart_send_param;
}

void sle_uart_start_scan(void)
{
    sle_seek_param_t param = { 0 };
    param.own_addr_type = 0;                              // 使用公共地址类型
    param.filter_duplicates = 0;                          // 不过滤重复设备，确保发现所有BMS
    param.seek_filter_policy = 0;                         // 接受所有设备的广播
    param.seek_phys = 1;                                  // 使用1M PHY物理层
    param.seek_type[0] = 1;                               // 主动扫描，发送扫描请求
    param.seek_interval[0] = SLE_SEEK_INTERVAL_DEFAULT;   // 扫描间隔100ms
    param.seek_window[0] = SLE_SEEK_WINDOW_DEFAULT;       // 扫描窗口100ms
    
    sle_set_seek_param(&param);  // 配置扫描参数
    sle_start_seek();            // 启动设备扫描
}

static void sle_uart_client_sample_sle_enable_cbk(errcode_t status)
{
    osal_printk("sle enable: %d.\r\n", status);
    sle_uart_client_init(sle_uart_notification_cb, sle_uart_indication_cb);
    sle_uart_start_scan();
}

static void sle_uart_client_sample_seek_enable_cbk(errcode_t status)
{
    if (status != 0) {
        osal_printk("%s sle_uart_client_sample_seek_enable_cbk,status error\r\n", SLE_UART_CLIENT_LOG);
    }
}

static void sle_uart_client_sample_seek_result_info_cbk(sle_seek_result_info_t *seek_result_data)
{
    // 打印发现设备的完整MAC地址
    osal_printk("sle_sample_seek_result_info_cbk  [%02x,%02x,%02x,%02x,%02x,%02x]\n",seek_result_data->addr.addr[0], 
    seek_result_data->addr.addr[1],seek_result_data->addr.addr[2],seek_result_data->addr.addr[3],seek_result_data->addr.addr[4],
    seek_result_data->addr.addr[5]);
    
    // 打印设备广播数据内容
    osal_printk("sle_sample_seek_result_info_cbk %s\r\n", seek_result_data->data);
    
    if (seek_result_data != NULL) {
        // 检查是否还能接受新的连接（最多支持8个BMS设备）
        if(g_sle_uart_conn_num < SLE_UART_CLIENT_MAX_CON){
            // 根据设备名称筛选目标BMS设备
            if (strstr((const char *)seek_result_data->data, SLE_UART_SERVER_NAME) != NULL) {
                osal_printk("will connect dev\n");
                // 保存目标设备地址信息
                (void)memcpy_s(&g_sle_uart_remote_addr, sizeof(sle_addr_t), &seek_result_data->addr, sizeof(sle_addr_t));
                // 找到目标设备后停止扫描
                sle_stop_seek();
            }
        }
    }
}

/**
 * @brief SLE扫描停止回调函数
 * @param status 扫描停止状态码
 * @details 当扫描停止后，尝试连接已发现的目标BMS设备
 */
static void sle_uart_client_sample_seek_disable_cbk(errcode_t status)
{
    if (status != 0) {
        osal_printk("%s sle_uart_client_sample_seek_disable_cbk,status error = %x\r\n", SLE_UART_CLIENT_LOG, status);
    } else {
        // 扫描停止成功，立即连接已发现的BMS设备
        sle_connect_remote_device(&g_sle_uart_remote_addr);
    }
}

static void sle_uart_client_sample_seek_cbk_register(void)
{
    g_sle_uart_seek_cbk.sle_enable_cb = sle_uart_client_sample_sle_enable_cbk;      // SLE协议栈使能回调
    g_sle_uart_seek_cbk.seek_enable_cb = sle_uart_client_sample_seek_enable_cbk;    // 扫描启用回调
    g_sle_uart_seek_cbk.seek_result_cb = sle_uart_client_sample_seek_result_info_cbk; // 扫描结果回调
    g_sle_uart_seek_cbk.seek_disable_cb = sle_uart_client_sample_seek_disable_cbk;  // 扫描停止回调
    sle_announce_seek_register_callbacks(&g_sle_uart_seek_cbk);                     // 注册到协议栈
}

static void sle_uart_client_sample_connect_state_changed_cbk(uint16_t conn_id, const sle_addr_t *addr,
                                                             sle_acb_state_t conn_state, sle_pair_state_t pair_state,
                                                             sle_disc_reason_t disc_reason)
{
    unused(pair_state);
    osal_printk("%s conn state changed disc_reason:0x%x\r\n", SLE_UART_CLIENT_LOG, disc_reason);
    
    if (conn_state == SLE_ACB_STATE_CONNECTED) {
        // === 连接建立成功处理 ===
        osal_printk("%s SLE_ACB_STATE_CONNECTED\r\n", SLE_UART_CLIENT_LOG);
        
        // 为新设备分配数组索引
        int device_index = allocate_device_index(conn_id, addr);
        if (device_index == -1) {
            printf("Failed to allocate device index for conn_id %d\r\n", conn_id);
            return;
        }

        // 保存新连接的ID到数组中
        g_sle_uart_conn_id[g_sle_uart_conn_num] = conn_id;
        
        // 启动SSAP信息交换流程
        ssap_exchange_info_t info = {0};
        info.mtu_size = SLE_MTU_SIZE_DEFAULT;               // 设置MTU大小为520字节
        info.version = 1;                                   // 设置协议版本
        ssapc_exchange_info_req(1, g_sle_uart_conn_id[g_sle_uart_conn_num], &info);
        
        // 增加已连接设备计数
        g_sle_uart_conn_num++;
        
    } else if (conn_state == SLE_ACB_STATE_NONE) {
        // === 连接状态为NONE ===
        osal_printk("%s SLE_ACB_STATE_NONE\r\n", SLE_UART_CLIENT_LOG);
        
    } else if (conn_state == SLE_ACB_STATE_DISCONNECTED) {
        // === 连接断开处理 ===
        osal_printk("%s SLE_ACB_STATE_DISCONNECTED\r\n", SLE_UART_CLIENT_LOG);
        g_sle_uart_conn_num--;                              // 减少连接计数
        remove_device_by_conn_id(conn_id);           // 移除断开连接的设备
        sle_uart_start_scan();                              // 重新启动扫描寻找设备
        
    } else {
        // === 未知状态错误 ===
        osal_printk("%s status error\r\n", SLE_UART_CLIENT_LOG);
    }
    
    // 连接状态变化后重新启动扫描（用于发现新设备）
    if (g_active_device_count < MAX_BMS_DEVICES) {
        sle_start_seek();
    }
}

/**
 * @brief SLE设备配对完成回调函数
 * @param conn_id 连接ID
 * @param addr 远程BMS设备地址
 * @param status 配对结果状态码
 * @details 配对成功后启动SSAP协议信息交换流程
 *          为后续的服务发现和数据通信做准备
 */
void  sle_uart_client_sample_pair_complete_cbk(uint16_t conn_id, const sle_addr_t *addr, errcode_t status)
{
    // 打印配对完成信息（显示部分地址用于调试）
    osal_printk("%s pair complete conn_id:%d, addr:%02x***%02x%02x\n", SLE_UART_CLIENT_LOG, conn_id,
                addr->addr[0], addr->addr[4], addr->addr[5]);
    
    if (status == 0) {
        // 配对成功，启动SSAP信息交换流程
        ssap_exchange_info_t info = {0};
        info.mtu_size = SLE_MTU_SIZE_DEFAULT;               // 设置默认MTU大小
        info.version = 1;                                   // 设置协议版本
        
        // 发起SSAP信息交换请求，为数据通信建立参数
        ssapc_exchange_info_req(0,  g_sle_uart_conn_id[g_sle_uart_conn_num], &info);
    }
}

static void sle_uart_client_sample_connect_cbk_register(void)
{
    g_sle_uart_connect_cbk.connect_state_changed_cb = sle_uart_client_sample_connect_state_changed_cbk;
    g_sle_uart_connect_cbk.pair_complete_cb =  sle_uart_client_sample_pair_complete_cbk;
    sle_connection_register_callbacks(&g_sle_uart_connect_cbk);
}

static void sle_uart_client_sample_exchange_info_cbk(uint8_t client_id, uint16_t conn_id, ssap_exchange_info_t *param,
                                                     errcode_t status)
{
    osal_printk("%s exchange_info_cbk,pair complete client id:%d status:%d\r\n",
                SLE_UART_CLIENT_LOG, client_id, status);
    osal_printk("%s exchange mtu, mtu size: %d, version: %d.\r\n", SLE_UART_CLIENT_LOG,
                param->mtu_size, param->version);
    
    // 配置服务发现参数
    ssapc_find_structure_param_t find_param = { 0 };
    find_param.type = SSAP_FIND_TYPE_PROPERTY;              // 查找属性类型
    find_param.start_hdl = 1;                               // 起始句柄（从1开始）
    find_param.end_hdl = 0xFFFF;                           // 结束句柄（最大值，搜索所有）
    
    // 启动服务结构发现，寻找BMS数据服务
    ssapc_find_structure(0, conn_id, &find_param);
}

/**
 * @brief 服务发现结果回调函数
 * @param client_id 客户端ID
 * @param conn_id 连接ID
 * @param service 发现的服务信息
 * @param status 操作状态码
 * @details 处理从BMS设备发现的GATT服务信息，保存服务句柄和UUID
 *          为后续的特征值读写操作提供地址信息
 */
static void sle_uart_client_sample_find_structure_cbk(uint8_t client_id, uint16_t conn_id,
                                                      ssapc_find_service_result_t *service,
                                                      errcode_t status)
{
    // 打印服务发现结果的详细信息
    osal_printk("%s find structure cbk client: %d conn_id:%d status: %d \r\n", SLE_UART_CLIENT_LOG,
                client_id, conn_id, status);
    osal_printk("%s find structure start_hdl:[0x%02x], end_hdl:[0x%02x], uuid len:%d\r\n", SLE_UART_CLIENT_LOG,
                service->start_hdl, service->end_hdl, service->uuid.len);
    
    // 保存发现的服务信息到全局变量，供后续操作使用
    g_sle_uart_find_service_result.start_hdl = service->start_hdl;
    g_sle_uart_find_service_result.end_hdl = service->end_hdl;
    memcpy_s(&g_sle_uart_find_service_result.uuid, sizeof(sle_uuid_t), &service->uuid, sizeof(sle_uuid_t));
}

static void sle_uart_client_sample_find_property_cbk(uint8_t client_id, uint16_t conn_id,
                                                     ssapc_find_property_result_t *property, errcode_t status)
{
    // 打印属性发现的详细信息
    osal_printk("%s sle_uart_client_sample_find_property_cbk, client id: %d, conn id: %d, operate ind: %d, "
                "descriptors count: %d status:%d property->handle %d\r\n", SLE_UART_CLIENT_LOG,
                client_id, conn_id, property->operate_indication,
                property->descriptors_count, status, property->handle);
    
    // 保存特征值句柄到发送参数结构，用于数据通信
    g_sle_uart_send_param.handle = property->handle;         // 保存特征值句柄
    g_sle_uart_send_param.type = SSAP_PROPERTY_TYPE_VALUE;   // 设置为值类型属性
}

/**
 * @brief 服务发现完成回调函数
 * @param client_id 客户端ID
 * @param conn_id 连接ID
 * @param structure_result 服务发现完成结果
 * @param status 操作状态码
 * @details 所有服务和特征值发现完成后的回调通知
 *          标志着GATT服务发现阶段的结束，可以开始数据通信
 */
static void sle_uart_client_sample_find_structure_cmp_cbk(uint8_t client_id, uint16_t conn_id,
                                                          ssapc_find_structure_result_t *structure_result,
                                                          errcode_t status)
{
    unused(conn_id);
    // 打印服务发现完成的详细信息
    osal_printk("%s sle_uart_client_sample_find_structure_cmp_cbk,client id:%d status:%d type:%d uuid len:%d \r\n",
                SLE_UART_CLIENT_LOG, client_id, status, structure_result->type, structure_result->uuid.len);
}

static void sle_uart_client_sample_write_cfm_cb(uint8_t client_id, uint16_t conn_id,
                                                ssapc_write_result_t *write_result, errcode_t status)
{
    // 打印写入确认的详细信息，包括句柄和操作类型
    osal_printk("%s sle_uart_client_sample_write_cfm_cb, conn_id:%d client id:%d status:%d handle:%02x type:%02x\r\n",
                SLE_UART_CLIENT_LOG, conn_id, client_id, status, write_result->handle, write_result->type);
}

static void sle_uart_client_sample_ssapc_cbk_register(ssapc_notification_callback notification_cb,
                                                      ssapc_indication_callback indication_cb)
{
    g_sle_uart_ssapc_cbk.exchange_info_cb = sle_uart_client_sample_exchange_info_cbk;
    g_sle_uart_ssapc_cbk.find_structure_cb = sle_uart_client_sample_find_structure_cbk;
    g_sle_uart_ssapc_cbk.ssapc_find_property_cbk = sle_uart_client_sample_find_property_cbk;
    g_sle_uart_ssapc_cbk.find_structure_cmp_cb = sle_uart_client_sample_find_structure_cmp_cbk;
    g_sle_uart_ssapc_cbk.write_cfm_cb = sle_uart_client_sample_write_cfm_cb;
    g_sle_uart_ssapc_cbk.notification_cb = notification_cb;
    g_sle_uart_ssapc_cbk.indication_cb = indication_cb;
    ssapc_register_callbacks(&g_sle_uart_ssapc_cbk);
}


void sle_uart_client_init(ssapc_notification_callback notification_cb, ssapc_indication_callback indication_cb)
{
    (void)osal_msleep(1000); /* 延时1s，等待SLE初始化完毕 */
    osal_printk("[SLE Client] try enable.\r\n");
    sle_uart_client_sample_seek_cbk_register();
    sle_uart_client_sample_connect_cbk_register();
    sle_uart_client_sample_ssapc_cbk_register(notification_cb, indication_cb);
    if (enable_sle() != ERRCODE_SUCC) {
        osal_printk("[SLE Client] sle enbale fail !\r\n");
    }
}


#include "app_init.h"
#include "pinctrl.h"
#include "uart.h"
// #include "pm_clock.h"
#include "sle_low_latency.h"
#define SLE_UART_TASK_STACK_SIZE            0x600
#include "sle_connection_manager.h"
#include "sle_ssap_client.h"
#include "sle_client.h"
#define SLE_UART_TASK_PRIO                  17
#define SLE_UART_TASK_DURATION_MS           2000


void sle_uart_notification_cb(uint8_t client_id, uint16_t conn_id, ssapc_handle_value_t *data,
    errcode_t status)
{
    // 准备消息队列数据结构
    notify_data_t msg_data = {0};
    
    // 为数据分配内存，确保异步处理时数据有效性
    void *buffer_cpy = osal_vmalloc(data->data_len);
    if (buffer_cpy == NULL) {
        osal_printk("Failed to allocate memory for buffer_cpy\r\n");
        return;
    }
    
    // 安全拷贝BMS数据到新分配的内存
    if (memcpy_s(buffer_cpy, data->data_len, data->data, data->data_len) != EOK) {
        osal_vfree(buffer_cpy);  // 拷贝失败时释放内存
        return;
    }
    
    // 封装消息数据
    msg_data.data = (uint8_t *)buffer_cpy;          // 数据指针
    msg_data.data_len = data->data_len;             // 数据长度
    msg_data.connect_id = conn_id;                  // 连接ID，标识BMS设备
    
    // 将消息写入队列，由专门的处理任务异步处理
    osal_msg_queue_write_copy(sle_msg_queue, (void *)&msg_data, sle_msg_size, 0);
    
    // 简单打印接收的数据长度，避免大量日志影响性能
    printf("Received notification data from conn_id: %d, length: %d\r\n", conn_id, data->data_len);
}

void sle_uart_indication_cb(uint8_t client_id, uint16_t conn_id, ssapc_handle_value_t *data,errcode_t status)
{
    unused(client_id);
    unused(conn_id);
    unused(status);
    
    // 打印接收到的指示数据（通常是重要信息）
    osal_printk("\n sle uart recived data : %s\r\n", data->data);
}

/**
 * @brief 处理BMS JSON数据并更新指定设备的数据
 * @param data JSON数据
 * @param data_len 数据长度
 * @param conn_id 连接ID
 */
static void process_bms_json_data(uint8_t *data, uint16_t data_len, uint16_t conn_id)
{
    // 查找设备索引
    int device_index = find_device_index_by_conn_id(conn_id);
    if (device_index < 0) {
        printf("Error: Could not find device index for conn_id %d\r\n", conn_id);
        return;
    }
    
    char *json_str = (char *)osal_vmalloc(data_len + 1);
    if (json_str == NULL) {
        printf("Failed to allocate memory for JSON string\r\n");
        return;
    }
    
    // 复制数据并添加字符串结束符
    if (memcpy_s(json_str, data_len + 1, data, data_len) != EOK) {
        osal_vfree(json_str);
        printf("Failed to copy JSON data\r\n");
        return;
    }
    json_str[data_len] = '\0';

    // 解析JSON数据
    cJSON *json = cJSON_Parse(json_str);
    if (json == NULL) {
        printf("JSON parse failed for device[%d]\r\n", device_index);
        osal_vfree(json_str);
        return;
    }


    // 解析total_voltage
    cJSON *total_voltage = cJSON_GetObjectItem(json, "total_voltage");
    if (cJSON_IsNumber(total_voltage)) {
        g_env_msg[device_index].total_voltage = total_voltage->valueint;
    }

    // 解析current
    cJSON *current = cJSON_GetObjectItem(json, "current");
    if (cJSON_IsNumber(current)) {
        g_env_msg[device_index].current = current->valueint;
    }

    // 解析cell_voltages数组
    cJSON *cell_voltages = cJSON_GetObjectItem(json, "cell_voltages");
    if (cJSON_IsArray(cell_voltages)) {
        int size = cJSON_GetArraySize(cell_voltages);
        int max_cells = sizeof(g_env_msg[device_index].cell_voltages)/sizeof(g_env_msg[device_index].cell_voltages[0]);
        for (int i = 0; i < size && i < max_cells; i++) {
            cJSON *item = cJSON_GetArrayItem(cell_voltages, i);
            if (cJSON_IsNumber(item)) {
                g_env_msg[device_index].cell_voltages[i] = item->valueint;
            }
        }
    }

    // 解析temperature数组
    cJSON *temperature = cJSON_GetObjectItem(json, "temperature");
    if (cJSON_IsArray(temperature)) {
        int size = cJSON_GetArraySize(temperature);
        int max_temps = sizeof(g_env_msg[device_index].temperature)/sizeof(g_env_msg[device_index].temperature[0]);
        for (int i = 0; i < size && i < max_temps; i++) {
            cJSON *item = cJSON_GetArrayItem(temperature, i);
            if (cJSON_IsNumber(item)) {
                g_env_msg[device_index].temperature[i] = item->valueint;
            }
        }
    }

    printf("Updated BMS[%d] data: conn_id=%d, total_voltage=%d, current=%d\r\n", 
           device_index, conn_id, g_env_msg[device_index].total_voltage, 
           g_env_msg[device_index].current);

    // 清理资源
    cJSON_Delete(json);
    osal_vfree(json_str);
}

static void *sle_client_task(const char *arg)
{
    unused(arg);
    sle_uart_client_init(sle_uart_notification_cb, sle_uart_indication_cb);
    while (1) {
        notify_data_t msg_data = {0};
        int msg_ret = osal_msg_queue_read_copy(sle_msg_queue, &msg_data, &sle_msg_size, OSAL_WAIT_FOREVER);
        
        if (msg_ret != OSAL_SUCCESS) {
            printf("sle_msg queue read copy fail.");
            continue;
        }
        
        if (msg_data.data != NULL) {
            process_bms_json_data(msg_data.data, msg_data.data_len, msg_data.connect_id);
            // 处理完成后释放数据内存
            osal_vfree(msg_data.data);
        } else {
            printf("sle_msg data is NULL.\r\n");
        }
    }
    return NULL;
}


/**
 * @brief 获取当前活跃设备数量
 * @return 当前活跃的BMS设备数量
 */
uint8_t get_active_device_count(void)
{
    return g_active_device_count;
}

static void sle_uart_entry(void)
{
    osal_task *task_handle = NULL;
    osal_kthread_lock();

    int ret = osal_msg_queue_create("sle_client", sle_msg_size, &sle_msg_queue, 0, sle_msg_size);
    if (ret != OSAL_SUCCESS) {
        printf("create sle_client queue failure!,error:%x\n", ret);
    }
    
    task_handle = osal_kthread_create((osal_kthread_handler)sle_client_task, 0, "SLEUartDongleTask",
                                      SLE_UART_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, SLE_UART_TASK_PRIO);
        osal_kfree(task_handle);
    }
    osal_kthread_unlock();
}

/* Run the sle_uart_entry. */
app_run(sle_uart_entry);