#ifndef PID_H
#define PID_H
#include "osal_debug.h"
#include "osal_task.h"
#include "securec.h"
#include "osal_addr.h"
#include "osal_wait.h"
#include "bsp_init.h"
#include "hal_bsp_dc.h"
// 电机选择
typedef enum {
    MOTOR_LEFT = 0x01, // 左电机
    MOTOR_RIGHT,       // 右电机
} Motor_t;
// 定义PID控制器结构体
typedef struct {
    float kp;         // 比例系数
    float ki;         // 积分系数
    float kd;         // 微分系数
    float prev_error; // 上一次的误差
    float integral;   // 误差积分
} PIDController;
extern PIDController left_pid, right_pid;

void PID_Init(void);

void pid_timer_init(void);
#endif