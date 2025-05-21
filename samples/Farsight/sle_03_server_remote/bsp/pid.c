#include "pid.h"
#include <stdio.h>

#include "timer.h"
#include "Track.h"
double dt = 0.05; // 采样时间间隔（秒）
PIDController left_pid, right_pid;
// 初始化PID控制器
void PID_Init(void)
{
    left_pid.kp = 120.0;
    left_pid.ki = 0.0;
    left_pid.kd = 0.0;
    left_pid.prev_error = 0;
    left_pid.integral = 0;

    right_pid.kp = 120.0;
    right_pid.ki = 0.0;
    right_pid.kd = 0.0;
    right_pid.prev_error = 0;
    right_pid.integral = 0;
}
// 计算 PID 输出
double PID_Compute(PIDController *pid, double targetSpeed, double actualSpeed, double dt)
{
    double error = targetSpeed - actualSpeed;
    pid->integral += error * dt;                        // 积分项累加
    double derivative = (error - pid->prev_error) / dt; // 计算微分项

    double output = pid->kp * error + pid->ki * pid->integral + pid->kd * derivative;
    pid->prev_error = error; // 存储误差值供下一次计算

    return output;
}
/**
 * @brief  读取左轮编码器
 * @note
 * @retval
 */
uint32_t motor_read_encoder(Motor_t motor)
{
    uint32_t temp;
    switch (motor) {
        case MOTOR_LEFT:
            temp = left_encoder_temp;
            left_encoder_temp = 0;
            break;
        case MOTOR_RIGHT:
            temp = right_encoder_temp;
            right_encoder_temp = 0;
            break;
        default:
            temp = 0;
            break;
    }
    return temp;
}
// 目标速度
#define pwm_max 40000
uint16_t change_flag = 1;
float target_speed = 1;
uint16_t flag;
timer_handle_t timer2_handle = 0;
void timer2_callback(uintptr_t data)
{
    unused(data);
    /* 读取编码器值 */
    systemValue.L_enc = motor_read_encoder(MOTOR_LEFT);
    systemValue.R_enc = motor_read_encoder(MOTOR_RIGHT);
    if (systemValue.car_status == CAR_STATUS_RUN || systemValue.car_status == CAR_STATUS_BACK ||
        systemValue.auto_track_flag) {

        // 计算左侧车轮的 PID 输出
        L_Motor_PWM += PID_Compute(&left_pid, target_speed, systemValue.L_enc, dt);
        // 计算右侧车轮的 PID 输出
        R_Motor_PWM += PID_Compute(&right_pid, target_speed, systemValue.R_enc, dt);

        if (L_Motor_PWM > pwm_max) {
            L_Motor_PWM = pwm_max;
        }
        if (R_Motor_PWM > pwm_max) {
            R_Motor_PWM = pwm_max;
        }
    } else {
        R_Motor_PWM = 0;
        L_Motor_PWM = 0;
    }

    if (systemValue.auto_track_flag) {
        Car_search_black_Line();
    } else {
        car_control_data(systemValue);
    }
    if (systemValue.car_status == CAR_STATUS_RUN || systemValue.car_status == CAR_STATUS_BACK ||
        systemValue.auto_track_flag == 1) {
        target_speed = 0.3 * change_flag + 7;
        if (flag % 2 == 0) {
            if (change_flag < Motor_PWM[systemValue.car_speed]) {
                change_flag++;
            }
        }

        flag++;
    }

    /* 开启下一次timer中断 */
    uapi_timer_start(timer2_handle, 15600, timer2_callback, 0);
}
void pid_timer_init(void)
{

    /* 设置 timer2 硬件初始化，设置中断号，配置优先级 */
    uapi_timer_adapter(TIMER_INDEX_2, TIMER_2_IRQN, 2);
    /* 创建 timer2 软件定时器控制句柄 */
    uapi_timer_create(TIMER_INDEX_2, &timer2_handle);
    uapi_timer_start(timer2_handle, 15600, timer2_callback, 0);
}
