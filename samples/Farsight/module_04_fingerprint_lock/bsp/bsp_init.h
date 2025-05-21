#ifndef UART_BSPINIT_H
#define UART_BSPINIT_H
#include "osal_debug.h"
#include "osal_task.h"
#include "securec.h"
#include "gpio.h"
#include "pinctrl.h"
#include "uart.h"
#include "osal_addr.h"
#include "osal_msgqueue.h"
#include "osal_wait.h"
#define DEIVER_BOARD 0
/* 串口接收数据结构体 */
typedef struct {
    uint8_t *value;
    uint16_t value_len;
} msg_data_t;
/* io */
#define USER_LED GPIO_13    // 板载led
#define USER_KEY GPIO_03    // 板载按键
#define FINGER_KEY GPIO_00  // 指纹模块触摸检测按键
#define FINGER_LOCK GPIO_02 // 指纹模块锁（led代替）
#define BEEP GPIO_10        // 板载蜂鸣器
#define UART_ID UART_BUS_2
// 锁的模式
typedef enum {
    KEY_PATTERN_INPUT = 0, // 录入
    KEY_PATTERN_TEST,      // 测试

} te_key_pattern_t;

// 状态
typedef enum {
    STATUS_OFF = 0, // 关闭
    STATUS_ON,      // 开启

} te_key_status_t;

/*********************************** 系统的全局变量 ***********************************/

typedef struct _system1_value {
    te_key_pattern_t key_pattern; // 指纹锁的模式
    te_key_status_t key_status;   // 指纹锁的状态;
    uint32_t lock_status;         // 按键标志位
    uint8_t finger_int_io;        // pcf8574 IO STATE
    uint8_t input_flag;           // 是否录入指纹标志
    uint8_t warning_flag;         // 录入 检测报警标志
} system_value_t;

extern system_value_t systemdata; // 系统全局变量
extern char oledShowBuff[50];
/* 消息队列结构体 */
extern unsigned long g_msg_queue;
extern unsigned int msg_rev_size;
void user_led_init(void);
void user_key_init(void);
void finger_io_init(void);
void oled_show(void);
void app_uart_init_config(void);
uint32_t uart_send_buff(uint8_t *str, uint16_t len);
void recv_finger_hex(msg_data_t *recv);
#endif