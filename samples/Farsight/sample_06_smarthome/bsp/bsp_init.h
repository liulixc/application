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
#include "mqtt.h"
#include "bsp_ili9341_4line.h"
#include "bsp_ft6336.h"
#include "font.h"
/* 通用io */
#define USER_LED GPIO_13
#define USER_BEEP GPIO_10
#define KEY GPIO_03 // 指纹模块触摸检测按键

void user_led_init(void);
void user_beep_init(void);

void KEY_Init(void);
void lcd_display(void);
#endif