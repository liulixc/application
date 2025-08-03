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


typedef struct {
    uint8_t recv[256]; // 与c文件保持一致
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
void L610_HuaweiCloudReport(char *topic, char *payload);

void L610_OpenSocket_TCP(char *server_ip, char *server_port);
void L610_SendMsgToTCPServer(char *msg);
void L610_OpenSocket_UDP(char *server_ip, char *server_port);
void L610_SendMsgToUDPServer(char *msg);
void L610_SendToken(char *token);
void L610_Reset(void);
int L610_PublishBMSDevices(const char *gate_report_topic, volatile void *g_env_msg, bool *is_device_active, uint8_t (*get_active_device_count)(void));

#endif