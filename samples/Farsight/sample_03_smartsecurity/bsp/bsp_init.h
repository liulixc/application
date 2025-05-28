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

void user_led_init(void);
void user_beep_init(void);
void set_buzzer(uint8_t status);
#endif