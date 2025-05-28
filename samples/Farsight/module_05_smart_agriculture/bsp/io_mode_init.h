#ifndef UART_IOINIT_H
#define UART_IOINIT_H
#include "osal_debug.h"
#include "osal_task.h"
#include "securec.h"
#include "gpio.h"
#include "pinctrl.h"
#include "pwm.h"
#include "tcxo.h"
#include "adc.h"
#include "adc_porting.h"
#define DEIVER_BOARD 0
/* io */
#define USER_KEY GPIO_03
#define USER_LED GPIO_13
#define USER_FLAME GPIO_00
#define USER_GAS GPIO_02
/* pwm */
#define USER_PWM0 GPIO_11
#define USER_PWM1 GPIO_09
#define PWM1_CHANNEL 1
#define PWM0_CHANNEL 3

typedef enum {
    SENSOR_ON = 0x01,
    SENSOR_OFF,
} te_flame_status_t;

typedef struct _system1_value {
    te_flame_status_t flame_status;
    te_flame_status_t gas_status;
    uint16_t CO2;
    uint16_t TVOC;
    uint16_t GAS_MIC;
    uint8_t pcf8574_int_io;
} system_value_t;
extern system_value_t systemdata;

extern uint8_t adcchannel;
extern uint8_t pwm_flag;
extern pwm_config_t PWM_LOW;
extern pwm_config_t PWM_HIGH;

void user_key_init(void);
void user_led_init(void);
void user_pwm_init(void);
void sensor_io_init(void);
void oled_show(void);
#endif