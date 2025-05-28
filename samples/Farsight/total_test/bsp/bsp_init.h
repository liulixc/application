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
#include "hal_bsp_sht20.h"
#include "hal_bsp_ssd1306.h"
/* io */
#define USER_LED GPIO_13
#define USER_BEEP GPIO_10
#define USER_KEY GPIO_03 // 板载按键

extern char oledShowBuff[50];
extern float temperature;
extern float humidity;
extern uint8_t nfc_test;
void user_led_init(void);
void oled_show(void);
void app_uart_init_config(void);
uint32_t uart_send_buff(uint8_t *str, uint16_t len);
void recv_uart_hex(void);
void user_beep_init(void);
void user_key_init(void);
#endif