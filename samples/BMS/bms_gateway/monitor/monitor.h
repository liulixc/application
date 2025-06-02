
#ifndef MONITOR_H
#define MONITOR_H

/* ������� */
#define MONITOR_TASK_PRIO 24
#define MONITOR_STACK_SIZE 0x2000
/* ���ڽ������ݽṹ�� */
typedef struct {
    uint8_t *value;
    uint16_t value_len;
} msg_data_t;
/* ���ڽ���io */
#define CONFIG_UART_TXD_PIN 15
#define CONFIG_UART_RXD_PIN 16
#define CONFIG_UART_PIN_MODE 1
#define CONFIG_UART_ID UART_BUS_1

#define MAX_WIFI_SSID_LEN 33     // 32���ַ� + ����ֹ��
#define MAX_WIFI_PASSWORD_LEN 65 // 64���ַ� + ����ֹ��

typedef enum MonitorCmdType {
    MONITOR_CMD_TYPE_NONE = 0, // ������
    MONITOR_CMD_TYPE_WIFI,    // ����WiFi

} MonitorCmdType;


#endif