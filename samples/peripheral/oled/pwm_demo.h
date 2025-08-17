#ifndef pwm_H
#define pwm_H


#define TEST_TCXO_DELAY_1000MS 1000    // 测试延时1000毫秒
#define PWM_TASK_PRIO 24               // PWM任务优先级
#define PWM_TASK_STACK_SIZE 0x1000     // PWM任务栈大小（4KB）

// WS2812 LED控制相关配置
#define CONFIG_PWM1_PIN 5              // PWM输出引脚号（GPIO5）
#define CONFIG_PWM1_CHANNEL 5          // PWM通道号
#define CONFIG_PWM1_PIN_MODE 1         // 引脚复用模式（PWM功能）
#define CONFIG_PWM1_GROUP_ID 0         // PWM组ID

#define CONFIG_PWM3_PIN 3              // PWM输出引脚号（GPIO5）
#define CONFIG_PWM3_CHANNEL 3          // PWM通道号
#define CONFIG_PWM3_PIN_MODE 1         // 引脚复用模式（PWM功能）
#define CONFIG_PWM3_GROUP_ID 3         // PWM组ID

// LED颜色和时序配置
#define COLOR 255                      // LED最大亮度值（8位：0-255）
#define DELAY_MS 1000                  // 颜色切换延时（毫秒）
#define RES 600                        // WS2812复位信号持续时间（微秒）

#include "common_def.h"        // 通用定义和宏
#include "pinctrl.h"           // 引脚控制接口
#include "pwm.h"               // PWM驱动接口
#include "soc_osal.h"          // 操作系统抽象层
#include "app_init.h"          // 应用初始化接口
#include "gpio.h"              // GPIO控制接口

void set_code0(void);
void set_code1(void);

void set_code00(void);
void set_code11(void);


void reset_leds(void);
void pwm_color(uint8_t red, uint8_t green, uint8_t blue);
void pwm_color1(uint8_t red, uint8_t green, uint8_t blue);



#endif