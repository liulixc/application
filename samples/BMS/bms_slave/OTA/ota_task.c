#include "string.h"
#include "lwip/netifapi.h"
#include "wifi_hotspot.h"
#include "wifi_hotspot_config.h"
#include "stdlib.h"
#include "uart.h"
#include "lwip/nettool/misc.h"
#include "soc_osal.h"
#include "app_init.h"
#include "cmsis_os2.h"
#include "wifi_device.h"
#include "wifi_event.h"
#include "lwip/sockets.h"
#include "lwip/ip4_addr.h"
#include "lwip/netdb.h"
#include "unistd.h"
#include "wifi_connect.h"
#include "upg.h"
#include "partition.h"
#include "upg_porting.h"
#include "ota_task.h"
#include "bms_slave.h"

#define OTA_TASK_STACK_SIZE 0x2000
#define RECV_BUFFER_SIZE     1024
#define DELAY_TIME_MS 100
#define HTTPC_DEMO_RECV_BUFSIZE 200  
#define SOCK_TARGET_PORT  7998

#define SSID  "QQ"
#define PASSWORD "tangyuan"

// #define SERVER_HOST   "quan.suning.com"
#define SERVER_IP     "1.13.92.135"//无法使用dns时采用手动ping解析域名

#define MAX_REQUEST_SIZE 512
static char g_request[MAX_REQUEST_SIZE] = 
    "GET /api/firmware/download/slave.fwpkg HTTP/1.1\r\n"
    "Host: 1.13.92.135:7998\r\n"  // 必须添加 Host 头
    "Connection: close\r\n"
    "\r\n";

char response[HTTPC_DEMO_RECV_BUFSIZE];

uint8_t recv_buffer[RECV_BUFFER_SIZE] = {0};

//*************************ota**************************//

errcode_t ota_prepare(uint32_t file_size)
{
    osal_printk("[oat task]:get in ota_task \r\n");
    uint32_t max_len;
    errcode_t ret = 0; 
    /* 依赖分区模块 */ 
    ret = uapi_partition_init(); 
    if (ret != 0)
    { 
        printf("[oat task]:uapi_partition_init error. ret = 0x%08x\r\n", ret);
        return -1;
    } 
    /* 1. 初始化update模块 */ 
    /* 2. 获取APP升级文件大小上限. */ 
    max_len = uapi_upg_get_storage_size(); 
    osal_printk("[ota task]: available storage size : %d\r\n",max_len);
    if(file_size > max_len)
    {
        osal_printk("[ota task]: file_size > max_len");
        return -1;
    }

    upg_prepare_info_t upg_prepare_info;
    upg_prepare_info.package_len = file_size;

    ret = uapi_upg_prepare(&upg_prepare_info);
    if(ret != ERRCODE_SUCC)
    {
        osal_printk("[oat task]: uapi_upg_prepare fail");
        return -1;
    }
    return ERRCODE_SUCC;
}

//-------------------------ota----------------------------//

//******************http**************************//


int http_clienti_get(const char *argument) {
    unused(argument);
    uint32_t total_recieved = 0;
    uint8_t head_recv = 0;
    uint16_t data_start_offset = 0;
    uint8_t first_check = 0;
    errcode_t ret = 0;
    uint32_t file_size = 0;

    wifi_connect(SSID,PASSWORD);
    struct sockaddr_in addr = {0};
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
    {
        osal_printk("socket init fail!!\r\n");
        return -1;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(SOCK_TARGET_PORT);
    addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) != 0) 
    {
        osal_printk("sock connect fail\r\n");
        lwip_close(sockfd);
        return -1;
    }
    osal_printk("sock connect succ\r\n");

    if (send(sockfd, g_request, strlen(g_request), 0) < 0) 
    {
        osal_printk("sock send fail\r\n");
        lwip_close(sockfd);
        return -1;
    }
    osal_printk("[ota task] : http send succ\r\n");

    // 响应头累积与解析
    static char header_buffer[2048] = {0};  // 增大缓冲区
    static int header_offset = 0;
    static int body_data_in_first_packet = 0;  // 第一个包中包含的响应体数据长度
    static int last_percent = -1;
    

    // 重置静态变量状态，避免上次失败的残留影响
    memset(header_buffer, 0, sizeof(header_buffer));
    header_offset = 0;
    body_data_in_first_packet = 0;
    last_percent = -1;  // 重置进度显示
    while (!head_recv) 
    {
        int bytes_received = recv(sockfd, recv_buffer, sizeof(recv_buffer), 0);
        if (bytes_received <= 0) 
        {
            osal_printk("[ota task] :1 http recv fail\r\n");
            lwip_close(sockfd);
            return -1;
        }
         upg_watchdog_kick();
        // 累积响应头
        if (header_offset + bytes_received < sizeof(header_buffer)) {
            memcpy(header_buffer + header_offset, recv_buffer, bytes_received);
            header_offset += bytes_received;
            header_buffer[header_offset] = '\0';  // 确保字符串结束
        }

        // 查找响应头结束标记 "\r\n\r\n"
        char *head_end = strstr(header_buffer, "\r\n\r\n");
        if (head_end) 
        {
            int header_length = (head_end - header_buffer) + 4;
            body_data_in_first_packet = header_offset - header_length;
            
            osal_printk("[ota task] : header length=%d, body data in first packet=%d\r\n", 
                       header_length, body_data_in_first_packet);
            
            // 使用 strstr 解析 Content-Length（按照开发指南建议）
            char *content_length_str = strstr(header_buffer, "Content-Length:");
            if (content_length_str) {
                content_length_str += strlen("Content-Length:");
                // 跳过空格
                while (*content_length_str == ' ' || *content_length_str == '\t') {
                    content_length_str++;
                }
                file_size = atoi(content_length_str);
                
                osal_printk("[ota task] : parsed Content-Length: %d\r\n", file_size);
                
                if (file_size > 0) {
                    head_recv = 1;
                    
                    // 执行OTA准备工作
                    if (ota_prepare(file_size) != ERRCODE_SUCC) 
                    {
                        osal_printk("[ota task] : ota prepare FAIL\r\n");
                        lwip_close(sockfd);
                        return -1;
                    }
                    
                    // 如果第一个包中包含响应体数据，先写入这部分数据
                    if (body_data_in_first_packet > 0) {
                        int write_size = body_data_in_first_packet;
                        if (write_size > file_size) {
                            write_size = file_size;
                        }
                        
                        osal_printk("[ota task] : writing first body data, size=%d\r\n", write_size);
                        
                        // 写入第一部分响应体数据
                        uapi_upg_write_package_sync(0, header_buffer + header_length, write_size);
                        total_recieved = write_size;
                        first_check = 1;  // 标记已处理第一个包
                        
                        osal_printk("[ota task] : first body data written, total_received=%d\r\n", total_recieved);
                    }
                } else {
                    osal_printk("[ota task] : invalid Content-Length\r\n");
                    lwip_close(sockfd);
                    return -1;
                }
            } else {
                osal_printk("[ota task] : Content-Length not found\r\n");
                lwip_close(sockfd);
                return -1;
            }
        }
    }

    // 接收响应体
    while (total_recieved < file_size) 
    {
        memset(recv_buffer, 0, sizeof(recv_buffer));  // 清空响应体缓冲区
        int bytes_received = recv(sockfd, recv_buffer, sizeof(recv_buffer), 0);
        
         upg_watchdog_kick();
        if (bytes_received == 0) {
            osal_printk("[ota task] : recv 0 bytes, connection closed\r\n");
            if (total_recieved == file_size) {
                osal_printk("[ota task] : all data received, breaking\r\n");
                break;
            } else {
                osal_printk("[ota task] : incomplete data, expected %d but got %d\r\n", file_size, total_recieved);
                break;
            }
        }
        if (bytes_received < 0) 
        {
            osal_printk("[ota task] :2 http recv fail\r\n");
            lwip_close(sockfd);
            return -1;
        }

        // 计算本次需写入的字节数（防止超量）
        int write_size = bytes_received;
        if (total_recieved + bytes_received > file_size) 
        {
            write_size = file_size - total_recieved;
            osal_printk("[ota task] : adjusting write_size from %d to %d\r\n", bytes_received, write_size);
        }
        
        // osal_printk("[ota task] : about to write %d bytes\r\n", write_size);

        // 写入接收到的数据（响应头已在前面处理）
        // osal_printk("[ota task] : write progress %d/%d, writing %d bytes\r\n", total_recieved, file_size, write_size);
        uapi_upg_write_package_sync(total_recieved, recv_buffer, write_size);
        total_recieved += write_size;
        // osal_printk("[ota task] : after write, total_received=%d\r\n", total_recieved);
        
        // 显示下载进度百分比
        int current_percent = (total_recieved * 100) / file_size;
        if (current_percent != last_percent) {
            osal_printk("[OTA进度] %d%%\r\n", current_percent);
            last_percent = current_percent;
        }

        // 超量保护（防止循环条件失效）
        if (total_recieved > file_size) 
        {
            total_recieved = file_size;
            break;
        }
    }

    osal_printk("[ota task] : recv all succ\r\n");
    
    // 数据完整性最终检查
    if (total_recieved != file_size) {
        osal_printk("[ota task] : data incomplete! expected=%d, received=%d\r\n", file_size, total_recieved);
        lwip_close(sockfd);
        return -1;
    }
    
    osal_printk("[ota task] : data integrity check passed, starting upgrade...\r\n");

    ret = uapi_upg_request_upgrade(false);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[ota task] : uapi_upg_request_upgrade error = 0x%x\r\n", ret);
        lwip_close(sockfd);
        return -1;
    }
    
    osal_printk("[ota task] : upgrade request successful, system will reboot...\r\n");

    upg_reboot();
    lwip_close(sockfd);
    return 0;
}

/**
 * @brief 更新固件下载路径
 * @param firmware_path 新的固件路径
 */
void ota_update_firmware_path(const char *firmware_path)
{
    if (firmware_path == NULL) {
        printf("[ota_update_firmware_path] Invalid firmware path\r\n");
        return;
    }
    
    // 构建新的HTTP请求
    snprintf(g_request, MAX_REQUEST_SIZE,
        "GET %s HTTP/1.1\r\n"
        "Host: %s:7998\r\n"
        "Connection: close\r\n"
        "\r\n",
        firmware_path, SERVER_IP);
    
    printf("[ota_update_firmware_path] Updated request: %s\r\n", g_request);
}
//---------------------http------------------------------//




/**
 * @brief 启动OTA任务
 * @return int 返回值，0表示成功，-1表示失败
 */
int ota_task_start(void)
{
    uint32_t ret;
    bms_thread_stop();
    osal_task *task_handle = NULL;
    // 加锁，防止多线程冲突
    osal_kthread_lock();
    // 创建MQTT主任务
    task_handle = osal_kthread_create((osal_kthread_handler)http_clienti_get, 0, "OTA_task", OTA_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, 17);
        osal_kfree(task_handle);
    }
    osal_kthread_unlock();
}


