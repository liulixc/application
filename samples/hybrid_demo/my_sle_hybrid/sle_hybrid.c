#include <stdio.h>   // 标准输入输出头文件
#include <string.h>   // 字符串操作头文件
#include <unistd.h>   // POSIX标准头文件
#include "app_init.h"  // 应用初始化头文件
#include "cmsis_os2.h"  // CMSIS-RTOS2 API头文件
#include "common_def.h"  // 通用定义头文件
#include "soc_osal.h"  // SOC操作系统头文件
#include "sle_device_discovery.h"  // SLE设备发现头文件
#include "sle_uuid_client.h"  // SLE客户端服务头文件
#include "sle_uuid_server.h"  // SLE服务器服务头文件
#include "sle_mesh.h"

/**
 * @brief 测试混合C模式发送数据
 * @note 等待字符串，每100毫秒发送一次
 */
static void TestHybridCSend(void)
{
    osal_printk("Hybrid-C Send\r\n");  // 打印混合C模式发送模式信息


    char data[32] = {0};  // 存储发送的数据
    int count = 1;  // 初始化计数器
    while (1)
    {
        // 将整数转换为字符串
        sprintf(data, "%d", count);
        // 通过混合C接口发送数据
        sle_hybridc_send_data((uint8_t *)data, strlen(data));
        count++;  // 增加计数器
        osDelay(100);  // 等待100毫秒
    }
}

/**
 * @brief 测试混合S模式发送数据
 * @note 等待混合服务器连接，每100毫秒发送一次
 */
static void TestHybridSSend(void)
{
    osal_printk("Hybrid-S Send\r\n");  // 打印混合S模式发送模式信息

    // 等待混合服务器连接，然后发送一次数据
    sle_hybrids_wait_client_connected();

    char data[16] = {0};  // 存储发送的数据
    int count = 1;  // 初始化计数器
    while (1)
    {
        // 将整数转换为字符串
        sprintf(data, "%d", count);
        // 通过混合S接口发送数据
        int ret = sle_hybrids_send_data((uint8_t *)data, strlen(data));
        // 如果发送失败，打印失败信息，否则打印成功信息
        if(ret != ERRCODE_SUCC)
        {
            osal_printk("sle_hybrids_send_data FAIL\r\n");  // 发送失败
        }
        else
        {
            osal_printk("sle_hybrids_send_data SUCC\r\n");  // 发送成功
        }
        count++;  // 增加计数器
        osDelay(100);  // 等待100毫秒
    }
}

static void sle_mesh_task(void)
{
    osal_printk("SLE Mesh Task Started\r\n");
    
    sle_mesh_init();

    char data[32] = {0};
    int count = 1;
    while (1) {
        sprintf(data, "Mesh message %d", count);
        osal_printk("Sending: %s\r\n", data);
        sle_mesh_send_data((uint8_t *)data, strlen(data));
        count++;
        osDelay(3000); // Send every 3 seconds
    }
}

// 外部函数，用于注册SLE通用回调
extern errcode_t sle_register_common_cbks(void);

/**
 * @brief SLE混合模式主任务
 * @param arg 任务参数，未使用
 * @note 此函数初始化客户端和服务器，注册回调，并启动SLE协议栈
 */
void sle_hybrid_task(char *arg)
{
    unused(arg);  // 避免未使用参数的编译告警
    errcode_t ret = 0;  // 用于保存函数返回值

    // 1. 初始化SLE服务器
    osal_printk("[sle hybrid] sle hybrid-s init\r\n");
    sle_hybrids_init();
    
    // 2. 初始化SLE客户端
    osal_printk("[sle hybrid] sle hybrid-c init\r\n");
    sle_hybridc_init();

    // 3. 注册SLE通用回调函数
    sle_register_common_cbks();

    // 4. 启用SLE协议栈
    ret = enable_sle();
    if (ret != 0)
    {
        osal_printk("enable_sle fail :%x\r\n", ret);
        return;  // 如果启用失败，直接返回
    }
    osal_printk("enable_sle succ\r\n");

    // 5. 启动Mesh测试任务
    sle_mesh_task();
}

// 任务优先级和堆栈大小定义
#define SLE_HYBRIDTASK_PRIO 24          // 混合模式任务优先级
#define SLE_HYBRID_STACK_SIZE 0x2000    // 混合模式任务堆栈大小(8KB)

/**
 * @brief SLE混合模式任务入口
 * @note 此函数用于启动混合模式任务
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