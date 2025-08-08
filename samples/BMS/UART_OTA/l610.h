#ifndef UART_BSPINIT_H
#define UART_BSPINIT_H
#include "osal_debug.h"
#include "osal_task.h"
#include "securec.h"
#include "gpio.h"
#include "pinctrl.h"
#include "uart.h"
#include "osal_addr.h"
#include "osal_wait.h"

// L610 OTA配置结构体
typedef struct {
    char firmware_url[256];
    char huawei_cloud_ip[64];
    char huawei_cloud_port[16];
    char client_id[64];
    char password[128];
    char command_topic[128];
} l610_ota_context_t;

// 命令响应topic模板
#define MQTT_CLIENT_RESPONSE "$oc/devices/680b91649314d11851158e8d_Battery01/sys/commands/response/request_id=%s"

typedef struct {
    uint8_t recv[1024]; // 与c文件保持一致

    uint16_t recv_len;
    uint8_t recv_flag;
} uart_recv;

void app_uart_init_config(void);
uint32_t uart_send_buff(uint8_t *str, uint16_t len);
void uart_read_handler(const void *buffer, uint16_t length, bool error);

void L610_SendCmd(char *cmd, char *result, uint32_t timeOut, uint8_t isPrintf);

void L610_Attach(uint8_t isPrintf, uint8_t isReboot);
void L610_Detach(uint8_t isPrintf);

void L610_HuaweiCloudConnect(char *ip, char *port, char *clientid, char *password, int keepalive, int cleanSession);
void L610_HuaweiCloudSubscribe(int qos, char *topic, uint8_t isPrintf);
char* l610_ota_extract_request_id(const char* topic);
int l610_ota_process_upgrade_command(const char* payload);
void l610_ota_handle_upgrade_command(char *command_data);



// HTTP协议相关函数
void L610_HttpSetUrl(char *url, uint8_t isPrintf);
void L610_HttpSetUserAgent(char *user_agent, uint8_t isPrintf);
void L610_HttpSetRange(char *range, uint8_t isPrintf);
void L610_HttpAction(int method, uint8_t isPrintf);
void L610_HttpRead(int offset, int length, uint8_t isPrintf);
void L610_HttpGet(char *url, char *user_agent, uint8_t isPrintf);
void L610_HttpGetWithRange(char *url, char *range, uint8_t isPrintf);

void L610_HuaweiCloudReport(char *topic, char *payload);

void L610_Reset(void);

// L610模块初始化状态管理函数
uint8_t L610_WaitForInit(uint32_t timeout_ms);

int L610_PublishBMSDevices(const char *gate_report_topic, volatile void *g_env_msg, bool *is_device_active, uint8_t (*get_active_device_count)(void));


// L610 OTA任务相关函数
void l610_ota_task(void);
void l610_ota_init(void);
void l610_ota_start_upgrade(char *firmware_url, int file_size);
void l610_ota_handle_upgrade_command(char *command_data);

#endif