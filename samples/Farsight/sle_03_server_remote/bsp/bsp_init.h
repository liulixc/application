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
#include "hal_bsp_dc.h"
#include "hal_bsp_pcf8574.h"
/* 通用io */
#define USER_BEEP GPIO_10
#define USER_KEY GPIO_03 // 板载按键
#define UART_ID UART_BUS_2
/* 串口接收数据结构体 */
typedef struct {
    uint8_t *value;
    uint16_t value_len;
} msg_data_t;
// 小车的当前状态值
typedef enum {
    CAR_STATUS_RUN = 0x01, // 前进
    CAR_STATUS_BACK,       // 后退
    CAR_STATUS_LEFT,       // 左转
    CAR_STATUS_RIGHT,      // 右转
    CAR_STATUS_STOP,       // 停止
    CAR_STATUS_L_SPEED,    // 低速行驶
    CAR_STATUS_M_SPEED,    // 中速行驶
    CAR_STATUS_H_SPEED,    // 高速行驶
} te_car_status_t;

typedef struct _system_value {
    te_car_status_t car_status; // 小车的状态
    uint16_t L_enc;             // 左电机的编码器值
    uint16_t R_enc;             // 右电机的编码器值
    uint16_t battery_voltage;   // 电池当前电压值
    uint16_t distance;          // 距离传感器
    uint8_t auto_track_flag; // 是否开启寻迹功能
    uint8_t car_speed;          // 小车档位
    uint8_t track_data;         // 寻迹通道
} system_value_t;

extern system_value_t systemValue; // 系统全局变量

typedef struct {
    uint8_t recv[50];
    uint8_t recv_len;
    uint8_t recv_flag;
} uart_recv;

extern uint16_t L_Motor_PWM;
extern uint16_t R_Motor_PWM;
extern uint16_t Motor_PWM[3];

extern char oledShowBuff[50];
extern float temperature;
extern float humidity;
extern char uart2_recv[30];
extern uint8_t recv_flag;
void user_key_init(void);
void user_led_init(void);
void oled_show(void);
void user_beep_init(void);
void car_control_data(system_value_t sys);

void app_uart_init_config(void);
#endif