
#ifndef MONITOR_H
#define MONITOR_H

/* ÈÎÎñÏà¹Ø */
#define MONITOR_TASK_PRIO 24
#define MONITOR_STACK_SIZE 0x2000
/* ´®¿Ú½ÓÊÕÊý¾Ý½á¹¹Ìå */
typedef struct {
    uint8_t *value;
    uint16_t value_len;
} msg_data_t;
/* ´®¿Ú½ÓÊÕio */
#define CONFIG_UART_TXD_PIN 15
#define CONFIG_UART_RXD_PIN 16
#define CONFIG_UART_PIN_MODE 1
#define CONFIG_UART_ID UART_BUS_1

#define MAX_WIFI_SSID_LEN 33     // 32¸ö×Ö·û + ¿ÕÖÕÖ¹·û
#define MAX_WIFI_PASSWORD_LEN 65 // 64¸ö×Ö·û + ¿ÕÖÕÖ¹·û

typedef enum MonitorCmdType {
    MONITOR_CMD_TYPE_NONE = 0, // ÎÞÃüÁî
    MONITOR_CMD_TYPE_WIFI,    // ÉèÖÃWiFi

} MonitorCmdType;


#endif