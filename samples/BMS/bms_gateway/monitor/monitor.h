
#ifndef MONITOR_H
#define MONITOR_H

/* 任务相关 */
#define MONITOR_TASK_PRIO 24
#define MONITOR_STACK_SIZE 0x2000
/* 串口接收数据结构体 */
typedef struct {
    uint8_t *value;
    uint16_t value_len;
} msg_data_t;
/* 串口接收io */
#define CONFIG_UART_TXD_PIN 15
#define CONFIG_UART_RXD_PIN 16
#define CONFIG_UART_PIN_MODE 1
#define CONFIG_UART_ID UART_BUS_1

#define MAX_WIFI_SSID_LEN 33     // 32个字符 + 空终止符
#define MAX_WIFI_PASSWORD_LEN 65 // 64个字符 + 空终止符

typedef enum MonitorCmdType {
    MONITOR_CMD_TYPE_NONE = 0, // 无命令
    MONITOR_CMD_TYPE_WIFI,    // 设置WiFi

} MonitorCmdType;


#endif