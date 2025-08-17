#ifndef pwm_H
#define pwm_H


#define TEST_TCXO_DELAY_1000MS 1000    // ������ʱ1000����
#define PWM_TASK_PRIO 24               // PWM�������ȼ�
#define PWM_TASK_STACK_SIZE 0x1000     // PWM����ջ��С��4KB��

// WS2812 LED�����������
#define CONFIG_PWM1_PIN 5              // PWM������źţ�GPIO5��
#define CONFIG_PWM1_CHANNEL 5          // PWMͨ����
#define CONFIG_PWM1_PIN_MODE 1         // ���Ÿ���ģʽ��PWM���ܣ�
#define CONFIG_PWM1_GROUP_ID 0         // PWM��ID

#define CONFIG_PWM3_PIN 3              // PWM������źţ�GPIO5��
#define CONFIG_PWM3_CHANNEL 3          // PWMͨ����
#define CONFIG_PWM3_PIN_MODE 1         // ���Ÿ���ģʽ��PWM���ܣ�
#define CONFIG_PWM3_GROUP_ID 3         // PWM��ID

// LED��ɫ��ʱ������
#define COLOR 255                      // LED�������ֵ��8λ��0-255��
#define DELAY_MS 1000                  // ��ɫ�л���ʱ�����룩
#define RES 600                        // WS2812��λ�źų���ʱ�䣨΢�룩

#include "common_def.h"        // ͨ�ö���ͺ�
#include "pinctrl.h"           // ���ſ��ƽӿ�
#include "pwm.h"               // PWM�����ӿ�
#include "soc_osal.h"          // ����ϵͳ�����
#include "app_init.h"          // Ӧ�ó�ʼ���ӿ�
#include "gpio.h"              // GPIO���ƽӿ�

void set_code0(void);
void set_code1(void);

void set_code00(void);
void set_code11(void);


void reset_leds(void);
void pwm_color(uint8_t red, uint8_t green, uint8_t blue);
void pwm_color1(uint8_t red, uint8_t green, uint8_t blue);



#endif