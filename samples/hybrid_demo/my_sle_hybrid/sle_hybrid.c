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

/**
 * @brief 测试混合模式下客户端发送数据功能
 * @note 采用计数字符串，每100毫秒发送一次递增数据
 */
static void TestHybridCSend(void)
{
    osal_printk("Hybrid-C Send\r\n");  // 打印客户端发送模式启动信息


    char data[32] = {0};  // 定义发送数据缓冲区
    int count = 1;  // 初始化计数器
    while (1)
    {
        // 将计数器转换为字符串
        sprintf(data, "%d", count);
        // 通过客户端接口发送数据
        sle_hybridc_send_data((uint8_t *)data, strlen(data));
        count++;  // 更新计数器
        osDelay(100);  // 延时100毫秒
    }
}

/**
 * @brief 测试混合模式下服务端发送数据功能
 * @note 等待客户连接后每100毫秒发送一次递增数据
 */
static void TestHybridSSend(void)
{
    osal_printk("Hybrid-S Send\r\n");  // 打印服务端发送模式启动信息

    // 等待客户端连接到服务端，这是一个阻塞操作
    sle_hybrids_wait_client_connected();

    char data[16] = {0};  // 定义发送数据缓冲区
    int count = 1;  // 初始化计数器
    while (1)
    {
        // 将计数器转换为字符串
        sprintf(data, "%d", count);
        // 通过服务端接口发送数据
        int ret = sle_hybrids_send_data((uint8_t *)data, strlen(data));
        // 数据发送结果打印调试日志
        if(ret != ERRCODE_SUCC)
        {
            osal_printk("sle_hybrids_send_data FAIL\r\n");  // 发送失败
        }
        else
        {
            osal_printk("sle_hybrids_send_data SUCC\r\n");  // 发送成功
        }
        count++;  // 更新计数器
        osDelay(100);  // 延时100毫秒
    }
}

// 外部函数声明，用于注册SLE通用回调函数
extern errcode_t sle_register_common_cbks(void);

/**
 * @brief SLE混合模式主任务函数
 * @param arg 传入参数，未使用
 * @note 依次初始化服务端、客户端，注册回调，启动发送测试
 */
void sle_hybrid_task(char *arg)
{
    unused(arg);  // 处理未使用的参数
    errcode_t ret = 0;  // 操作返回状态码
    
    // 1. 初始化SLE服务端
    osal_printk("[sle hybrid] sle hybrid-s init\r\n");
    sle_hybrids_init();
    // 设置远程服务器名称，用于客户端连接时判断
    sle_set_server_name("sle_server");
    
    // 2. 初始化SLE客户端
    osal_printk("[sle hybrid] sle hybrid-c init\r\n");
    sle_hybridc_init();

    // 3. 注册SLE通用回调函数
    sle_register_common_cbks();

    // 4. 启用SLE服务
    ret = enable_sle();
    if (ret != 0)
    {
        osal_printk("enable_sle fail :%x\r\n", ret);
        return;  // 启用失败直接返回
    }
    osal_printk("enable_sle succ\r\n");
    // 5. 设置客户端地址
    sle_set_hybridc_addr();

    // 6. 选择测试模式：客户端发送或服务端发送
    // TestHybridCSend();  // 当前注释掉，不使用客户端发送测试
    TestHybridSSend();  // 使用服务端发送测试
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