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
extern uint32_t left_encoder_temp, right_encoder_temp; // 左电机和右电机的编码器脉冲计数值 - 临时值
void user_pwm_init(void);
void user_pwm_change(uint32_t l_pwm, uint32_t r_pwm);
void car_io_init(void);
void user_m1_fan(void);
void user_m1_zheng(void);
void user_m2_fan(void);
void user_m2_zheng(void);
void car_stop(void);
void car_advance(uint32_t l_pwm, uint32_t r_pwm);
void car_back(uint32_t l_pwm, uint32_t r_pwm);
void car_turn_left(void);
void car_turn_right(void);
void motor_enc_init(void);
#endif