#include "string.h"
#include "stdlib.h"
#include "uart.h"
#include "soc_osal.h"
#include "app_init.h"
#include "cmsis_os2.h"
#include "unistd.h"
#include "upg.h"
#include "partition.h"
#include "upg_porting.h"
#include "gpio.h"
#include "pinctrl.h"
#include "osal_addr.h"
#include "osal_wait.h"
#include "securec.h"
#include "l610.h"
#include "stdio.h"

#define L610_OTA_TASK_STACK_SIZE 0x3000
#define L610_OTA_CHUNK_SIZE      360000  // 36万字节


// 全局OTA上下文
static l610_ota_context_t g_l610_ota_ctx = {
    .firmware_url = "http://1.13.92.135:7998/api/firmware/download/slave.fwpkg",
    .huawei_cloud_ip = "117.78.5.125",
    .huawei_cloud_port = "1883",
    .client_id = "680b91649314d11851158e8d_Battery01",
    .password = "12345678",
    .command_topic = "$oc/devices/680b91649314d11851158e8d_Battery01/sys/commands/#",
};

// 全局变量
static uint32_t total_file_size = 0;
static uint32_t received_size = 0;
// 外部UART接收结构体引用
extern uart_recv uart2_recv;

/**
 * @brief L610 OTA准备函数
 * @param file_size 固件文件大小
 * @return ERRCODE_SUCC-成功, 其他-失败
 */
errcode_t l610_ota_prepare(uint32_t file_size)
{
    osal_printk("[L610 OTA]: Preparing OTA upgrade, file size: %u bytes\r\n", file_size);
    
    uint32_t max_len;
    errcode_t ret = 0;
    
    // 依赖分区模块
    ret = uapi_partition_init();
    if (ret != 0) {
        osal_printk("[L610 OTA]: partition init error\r\n");
        return -1;
    }
    
    // 获取APP升级文件大小上限
    max_len = uapi_upg_get_storage_size();
    
    osal_printk("[L610 OTA]: Available storage size: %u bytes\r\n", max_len);
    
    if (file_size > max_len) {
        osal_printk("[L610 OTA]: file size too large (file: %u > max: %u)\r\n", file_size, max_len);
        return -1;
    }
    
    upg_prepare_info_t upg_prepare_info;
    upg_prepare_info.package_len = file_size;
    
    ret = uapi_upg_prepare(&upg_prepare_info);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[L610 OTA]: prepare failed\r\n");
        return -1;
    }
    
    osal_printk("[L610 OTA]: OTA prepare successful for %u bytes\r\n", file_size);
    return ERRCODE_SUCC;
}


// Static variables for robust HTTP header and data parsing
static char http_buffer[2048];
static int http_buffer_len = 0;
static bool header_parsed = false;
static int actual_header_length = 0;  // 实际HTTP响应头长度

/**
 * @brief L610 OTA初始化函数
 */
void l610_ota_init(void)
{
    osal_printk("[L610 OTA]: Initializing L610 OTA module\r\n");
    
    received_size = 0;
    total_file_size = 0;

    // Reset parsing state for new OTA attempt
    http_buffer_len = 0;
    header_parsed = false;

    app_uart_init_config();

    if (!L610_WaitForInit(10000)) {
        osal_printk("[L610 OTA]: L610 init failed. Halting.\r\n");
        return;
    }

    L610_Attach(1, 1);
    L610_HuaweiCloudConnect(
        g_l610_ota_ctx.huawei_cloud_ip,
        g_l610_ota_ctx.huawei_cloud_port,
        g_l610_ota_ctx.client_id,
        g_l610_ota_ctx.password,
        60,
        0
    );

    L610_HuaweiCloudSubscribe(0, g_l610_ota_ctx.command_topic, 1);    
    osal_msleep(1000);
}


/**
 * @brief L610 OTA任务线程函数
 * @param argument 任务参数
 */
void l610_ota_task_thread(void *argument)
{
    unused(argument);
    // 初始化
    l610_ota_init();

    osal_printk("[L610 OTA]: Waiting for upgrade command...\r\n");
    while (1) {
        if (uart2_recv.recv_flag) {
            char* message = (char*)uart2_recv.recv;
            if (strstr(message, "upgrade") || strstr(message, "OTA")) {
                osal_printk("[L610 OTA]: Upgrade command received\r\n");
                l610_ota_handle_upgrade_command(message);
                uart2_recv.recv_flag = 0;
                break; 
            }
            uart2_recv.recv_flag = 0;
        }
        osal_msleep(100);
        upg_watchdog_kick();
    }
    
    osal_printk("[L610 OTA]: Starting firmware download...\r\n");    

    memset(uart2_recv.recv, 0, sizeof(uart2_recv.recv));
    uart2_recv.recv_flag = 0;
    http_buffer_len = 0;
    header_parsed = false;

    uint8_t head_recv = 0;
    total_file_size=980624;
    // 分块下载剩余数据
     while (received_size < total_file_size) {
         uint32_t chunk_start = received_size;
         uint32_t chunk_end = chunk_start + L610_OTA_CHUNK_SIZE - 1;
         if (chunk_end >= total_file_size) {
             chunk_end = total_file_size - 1;
         }
        
        osal_printk("[L610 OTA]: Downloading chunk %u-%u\r\n", chunk_start, chunk_end);
        memset(uart2_recv.recv, 0, sizeof(uart2_recv.recv));
        uart2_recv.recv_flag = 0;
        
        char range_header[64];
        snprintf(range_header, sizeof(range_header), "bytes=%u-%u", chunk_start, chunk_end);
        L610_HttpGetWithRange(g_l610_ota_ctx.firmware_url, range_header, 1);
        L610_HttpRead(0,0,1);
        
        uint32_t start_received_size = received_size;
        uint8_t chunk_header_parsed = 0;
        
        uint32_t chunk_target_size = chunk_end - chunk_start + 1;
        uint32_t chunk_received = 0;
        uint32_t chunk_timeout_count = 0;
        uint32_t chunk_max_timeout = 10;
        
        while (chunk_received < chunk_target_size && received_size < total_file_size) {
            if (uart2_recv.recv_flag) {
                chunk_timeout_count = 0; // 重置超时计数
                char* p_http_read = strstr((char*)uart2_recv.recv, "+HTTPREAD:");
                if (p_http_read) {
                    char* data_start = strchr(p_http_read, '\n');
                    if (data_start) {
                        data_start++; // 跳过换行符
                        int data_len = uart2_recv.recv_len - (data_start - (char*)uart2_recv.recv);
                        
                        if (!chunk_header_parsed) {
                            // 查找响应头结束标记，跳过HTTP响应头
                            char* header_end = strstr(data_start, "\r\n\r\n");
                            if (header_end) {
                                chunk_header_parsed = 1;
                                header_end += 4; // 跳过"\r\n\r\n"
                                int header_length = header_end - data_start;
                                int body_data_len = data_len - header_length;
                                
                                if (body_data_len > 0) {
                                    int write_size = body_data_len;
                                    if (chunk_received + write_size > chunk_target_size) {
                                        write_size = chunk_target_size - chunk_received;
                                    }
                                    if (received_size + write_size > total_file_size) {
                                        write_size = total_file_size - received_size;
                                    }
                                    
                                    if (write_size > 0) {
                                        uapi_upg_write_package_sync(received_size, (uint8_t*)header_end, write_size);
                                        received_size += write_size;
                                        chunk_received += write_size;
                                        
                                        osal_printk("[L610 OTA]: Chunk data (after header) %d bytes, chunk: %u/%u, total: %u/%u\r\n", 
                                                   write_size, chunk_received, chunk_target_size, received_size, total_file_size);
                                    }
                                }
                            }
                        } else {
                            int write_size = data_len;
                            if (chunk_received + write_size > chunk_target_size) {
                                write_size = chunk_target_size - chunk_received;
                            }
                            if (received_size + write_size > total_file_size) {
                                write_size = total_file_size - received_size;
                            }
                            
                            if (write_size > 0) {
                                uapi_upg_write_package_sync(received_size, (uint8_t*)data_start, write_size);
                                received_size += write_size;
                                chunk_received += write_size;
                                
                                osal_printk("[L610 OTA]: Chunk data %d bytes, chunk: %u/%u, total: %u/%u\r\n", 
                                           write_size, chunk_received, chunk_target_size, received_size, total_file_size);
                            }
                        }
                    }
                } else {
                    // 直接处理数据（没有+HTTPREAD:前缀）
                    int write_size = uart2_recv.recv_len;
                    if (chunk_received + write_size > chunk_target_size) {
                        write_size = chunk_target_size - chunk_received;
                    }
                    if (received_size + write_size > total_file_size) {
                        write_size = total_file_size - received_size;
                    }
                    
                    if (write_size > 0) {
                        uapi_upg_write_package_sync(received_size, uart2_recv.recv, write_size);
                        received_size += write_size;
                        chunk_received += write_size;
                        
                        osal_printk("[L610 OTA]: Chunk data (direct) %d bytes, chunk: %u/%u, total: %u/%u\r\n", 
                                   write_size, chunk_received, chunk_target_size, received_size, total_file_size);
                    }
                }
                uart2_recv.recv_flag = 0;
            } else {
                // 没有数据时增加超时计数
                chunk_timeout_count++;
                if (chunk_timeout_count >= chunk_max_timeout) {
                    osal_printk("[L610 OTA]: Chunk download timeout, breaking loop. Chunk received: %u/%u bytes\r\n", 
                               chunk_received, chunk_target_size);
                    break;
                }
                osal_msleep(1); // 等待1毫秒
            }
            upg_watchdog_kick();
        }
        
        osal_printk("[L610 OTA]: Chunk download completed. Chunk: %u/%u bytes, Total: %u/%u\r\n", 
                   chunk_received, chunk_target_size, received_size, total_file_size);
    }


    
    if(received_size>=total_file_size){
        received_size=total_file_size;
    }
    
    // 升级流程
    if (received_size >= total_file_size) {
        osal_printk("[L610 OTA]: recv all data succ, total size: %u\r\n", total_file_size);
        errcode_t ret = uapi_upg_request_upgrade(false);
        if (ret == ERRCODE_SUCC) {
            osal_printk("[L610 OTA]: Upgrade request successful. Rebooting...\r\n");
            osal_msleep(1000);
            upg_reboot();
        } else {
            osal_printk("[L610 OTA]: Upgrade request failed. Error: 0x%x\r\n", ret);
        }
    } else {
        osal_printk("[L610 OTA]: Data incomplete. Halting.\r\n");
    }
}

static void l610_ota_sample(void)
{
    osThreadAttr_t attr;
    attr.name = "l610_ota_task";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = L610_OTA_TASK_STACK_SIZE;
    attr.priority = osPriorityNormal;
    
    if (osThreadNew((osThreadFunc_t)l610_ota_task_thread, NULL, &attr) == NULL) {
        osal_printk("[L610 OTA]: Create l610_ota_task fail.\r\n");
    } else {
        osal_printk("[L610 OTA]: Create l610_ota_task success.\r\n");
    }
}

app_run(l610_ota_sample);