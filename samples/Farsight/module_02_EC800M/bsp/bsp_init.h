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
#include "hal_bsp_dc.h"
#define DEIVER_BOARD 1
#define COEFFICIENT_1000 1000.0
/* 通用io */
#define USER_LED GPIO_13
#define USER_BEEP GPIO_10
// 串口id
#define UART_ID UART_BUS_2
// 串口接收
#define UART_RECV_SIZE 512
typedef struct {
    uint8_t recv[UART_RECV_SIZE];
    uint8_t recv_len;
    uint8_t recv_flag;
} uart_recv;
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

/*********************************** 系统的全局变量 ***********************************/
typedef struct _system_value {
    te_car_status_t car_status; // 小车的状态
    uint16_t left_motor_speed;  // 左电机的编码器值
    uint16_t right_motor_speed; // 右电机的编码器值
    uint16_t battery_voltage;   // 电池当前电压值
    uint16_t distance;          // 距离传感器
    uint8_t auto_abstacle_flag; // 是否开启避障功能
    int udp_socket_fd;          // UDP通信的套接字
} system_value_t;
extern system_value_t systemValue; // 系统全局变量

extern uart_recv uart2_recv;
extern char oledShowBuff[50];
extern uint8_t latitude[20];
extern uint8_t longitude[20];
extern float temperature;
extern float humidity;
void user_led_init(void);
char *get_CurrentCarStatus(system_value_t sys);
void oled_show(void);
void app_uart_init_config(void);
uint32_t uart_send_buff(uint8_t *str, uint16_t len);
void control_data(system_value_t sys);
#endif