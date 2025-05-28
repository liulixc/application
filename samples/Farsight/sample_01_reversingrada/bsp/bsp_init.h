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
#include "hal_bsp_ssd1306.h"
/* 通用io */
#define USER_LED GPIO_13
#define USER_BEEP GPIO_10
// 串口id
#define UART_ID UART_BUS_2
extern char uart2_recv[30];
extern uint16_t distance;
void user_led_init(void);
void app_uart_init_config(void);
void recv_uart_hex(void);
void user_beep_init(void);
#endif