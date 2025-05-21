
#ifndef HAL_BSP_IR_H
#define HAL_BSP_IR_H
#include "osal_debug.h"
#include "osal_task.h"
#include "securec.h"
#include "gpio.h"
#include "pinctrl.h"
#include "timer.h"
#include "osal_addr.h"
#include "osal_wait.h"
#include "pwm.h"
#define USER_PWM5 GPIO_05
#define PWM_CHANNEL5 5
void user_ir_start(void);
void ir_Encode(uint8_t user, uint8_t key);
void ir_send_str(char *buf);
#endif