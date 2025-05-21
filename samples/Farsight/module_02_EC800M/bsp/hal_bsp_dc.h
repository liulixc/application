#ifndef DC_H
#define DC_H
#include "pwm.h"
#include "osal_debug.h"
#include "osal_task.h"
#include "securec.h"
#include "gpio.h"
#include "pinctrl.h"
// pwm io
#define USER_PWM0 GPIO_00
#define USER_PWM1 GPIO_01
#define PWM_CHANNEL0 0
#define PWM_CHANNEL1 1
// 电机转向io
#define IN0_M1 GPIO_09
#define IN1_M1 GPIO_12

#define IN0_M2 GPIO_05
#define IN1_M2 GPIO_14
// 测速io
#define M1_ENC GPIO_06
#define M2_ENC GPIO_11

extern uint8_t dcspeed_flag;
extern pwm_config_t PWM_level[3];
void user_pwm_init(void);
void car_io_init(void);
void user_m1_inverse(void);
void user_m1_positive(void);
void user_m2_inverse(void);
void user_m2_positive(void);

void car_stop(void);
void car_advance(void);
void car_back(void);
void car_turn_left(uint16_t delay_time);
void car_turn_right(uint16_t delay_time);

#endif