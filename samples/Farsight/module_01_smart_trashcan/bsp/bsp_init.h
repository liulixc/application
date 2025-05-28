#ifndef UART_BSPINIT_H
#define UART_BSPINIT_H
#include "osal_debug.h"
#include "osal_task.h"
#include "securec.h"
#include "gpio.h"
#include "pinctrl.h"
#include "uart.h"
#include "osal_addr.h"
#include "osal_msgqueue.h"
#include "osal_wait.h"
#include "pwm.h"

/* io */
#define USER_LED GPIO_13
#define USER_KEY GPIO_03
#define USER_PWM GPIO_11
#define PWM_CHANNEL 3
#define UART_ID UART_BUS_2

/*********************************** 系统的全局变量 ***********************************/
/* 雷达的当前状态值 */
typedef enum {
    Radar_STATUS_OFF = 0, // 关闭
    Radar_STATUS_ON,      // 开启

} te_radar_status_t;
typedef struct _system_value {
    te_radar_status_t radar_status; // 雷达的状态
    uint32_t distance;
} system_value_t;
extern system_value_t systemdata; // 系统全局变量
extern char oledShowBuff[50];
/* 消息队列结构体 */
extern unsigned long g_msg_queue;
extern unsigned int msg_rev_size;
void user_led_init(void);
void oled_show(void);
void app_uart_init_config(void);
uint32_t uart_send_buff(uint8_t *str, uint16_t len);
void recv_uart_hex(void);
void user_pwm_init(void);
#endif