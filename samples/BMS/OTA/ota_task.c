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
#include "errno.h"
#include "wifi_connect.h"
#include "upg.h"
#include "partition.h"
#include "upg_porting.h"

#define WIFI_TASK_STACK_SIZE 0x2000
#define RECV_BUFFER_SIZE     1024
#define DELAY_TIME_MS 100
#define HTTPC_DEMO_RECV_BUFSIZE 200  
#define SOCK_TARGET_PORT  8082
#define RECEIVE_TIMEOUT_TV_SEC 10  // 接收超时10秒
#define RECEIVE_TIMEOUT_TV_USEC 0  // 接收超时0微秒  

#define CONFIG_WIFI_SSID "QQ" // 要连接的WiFi热点账号
#define CONFIG_WIFI_PWD "tangyuan" // 要连接的WiFi热点密码

#define SERVER_HOST   "quan.suning.com"
#define SERVER_IP     "172.30.28.86"//无法使用dns时采用手动ping解析域名
static const char *g_request = 
    "GET /test.fwpkg HTTP/1.1\r\n"
    "Host: 172.30.28.86:8082\r\n"  // 必须添加 Host 头
    "Connection: close\r\n"
    "\r\n";char response[HTTPC_DEMO_RECV_BUFSIZE];
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
    osal_printk("[oat task]:availaibe  storage size : %d\r\n",max_len);
    if(file_size > max_len)
    {
        osal_printk("[oat task]: file_size > max_len");
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

    struct sockaddr_in addr = {0};

    wifi_connect(CONFIG_WIFI_SSID, CONFIG_WIFI_PWD);
    
    // WiFi连接后等待网络完全就绪
    osal_printk("[ota task]: Waiting for network ready...\r\n");
    osDelay(3000);  // 等待3秒确保网络稳定
    

    addr.sin_family = AF_INET;
    addr.sin_port = PP_HTONS(SOCK_TARGET_PORT);
    addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    int s = socket(AF_INET, SOCK_STREAM, 0);
    osal_printk("s = %d\r\n", s);
    if (s < 0)
    {
        return;
    }

    // socket连接服务器
     osal_printk("NO1:... allocated socket\r\n");
     if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) != 0)
     {
         osal_printk("... socket connect failed errno=%d", errno);
         lwip_close(s);
         return -1;
     }
     osal_printk("NO2:... connected\r\n");
     
     // 发送HTTP GET请求
     if (lwip_write(s, g_request, strlen(g_request)) < 0)
     {
         lwip_close(s);
         return -1;
     }
     osal_printk("NO3:... socket send success\r\n");
 
     // 设置接收超时
     struct timeval receiving_timeout;
     receiving_timeout.tv_sec = RECEIVE_TIMEOUT_TV_SEC;
     receiving_timeout.tv_usec = RECEIVE_TIMEOUT_TV_USEC;
     if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout, sizeof(receiving_timeout)) < 0)
     {
         osal_printk("... failed to set socket receiving timeout\r\n");
         lwip_close(s);
         return -1;
     }
     osal_printk("NO4:... set socket receiving timeout success\r\n");

    // 响应头累积与解析
    static char header_buffer[RECV_BUFFER_SIZE] = {0};
    static int header_offset = 0;
    while (!head_recv) 
    {
        int bytes_received = lwip_read(s, recv_buffer, sizeof(recv_buffer));
        osal_printk("Received header:\r\n%s\r\n",header_buffer);
        if (bytes_received <= 0) 
        {
            osal_printk("[ota task] :1 http recv fail\r\n");
            lwip_close(s);
            return -1;
        }
        upg_watchdog_kick();

        // 累积响应头
        memcpy(header_buffer + header_offset, recv_buffer, bytes_received);
        header_offset += bytes_received;

        // 查找响应头结束标记
        char *head_end = osal_strstr(header_buffer, "\r\n\r\n");
        if (head_end) 
        {
            data_start_offset = (head_end - header_buffer) + 4;
            // 解析 Content-Length
            char *content_length_str = osal_strstr(header_buffer, "Content-Length:");
            if (content_length_str) {
                content_length_str += strlen("Content-Length:");
                char *endptr = NULL;
                file_size = strtol(content_length_str, &endptr, 10);
                if (endptr && (*endptr == '\r' || *endptr == '\n' || isspace(*endptr))) 
                {
                    head_recv = 1;
                    osal_printk("[ota task] : recv http data length: %d\r\n", file_size);
                    if (ota_prepare(file_size) != ERRCODE_SUCC) 
                    {
                        osal_printk("[ota task] : ota prepare FAIL\r\n");
                        lwip_close(s);
                        return -1;
                    }
                }
            }
            // 重置响应头缓冲区
            memset(header_buffer, 0, RECV_BUFFER_SIZE);
            header_offset = 0;
        }
    }

    // 接收响应体
    while (total_recieved < file_size) 
    {
        memset(recv_buffer, 0, sizeof(recv_buffer));  // 清空响应体缓冲区
        int bytes_received = lwip_read(s, recv_buffer, sizeof(recv_buffer));
        upg_watchdog_kick();
        if (bytes_received == 0 && total_recieved == file_size) 
            break;
        if (bytes_received < 0) 
        {
            osal_printk("[ota task] :2 http recv fail\r\n");
            lwip_close(s);
            return -1;
        }

        // 计算本次需写入的字节数（防止超量）
        int write_size = bytes_received;
        if (total_recieved + bytes_received > file_size) 
        {
            write_size = file_size - total_recieved;
        }

        if (first_check == 0) 
        {
            // 第一次写入（跳过响应头）
            uapi_upg_write_package_sync(
                total_recieved,
                recv_buffer + data_start_offset,
                write_size
            );
            total_recieved += (write_size);
            first_check = 1;
        } 
        else 
        {
            // 后续写入全部有效数据
            uapi_upg_write_package_sync(total_recieved, recv_buffer, write_size);
            total_recieved += write_size;
        }

        // 超量保护（防止循环条件失效）
        if (total_recieved > file_size) 
        {
            total_recieved = file_size;
            break;
        }
    }

    osal_printk("[ota task] : recv all succ\r\n");

    ret = uapi_upg_request_upgrade(false);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[ota task] : uapi_upg_request_upgrade error = 0x%x\r\n", ret);
        lwip_close(s);
        return -1;
    }

    upg_reboot();
    lwip_close(s);
    return 0;
}
//---------------------http------------------------------//


static void TCP_sample(void)
{
    osThreadAttr_t attr;
    attr.name = "TCP_sample_task";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = WIFI_TASK_STACK_SIZE;
    attr.priority = osPriorityNormal;
    if (osThreadNew((osThreadFunc_t)http_clienti_get, NULL, &attr) == NULL) {
        printf("Create sta_sample_task fail.\r\n");
    }
    printf("Create sta_sample_task succ.\r\n");
}
app_run(TCP_sample);