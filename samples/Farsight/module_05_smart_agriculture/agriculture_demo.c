/*
 * Copyright (c) 2023 Beijing HuaQing YuanJian Education Technology Co., Ltd
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "stdio.h"
#include "string.h"
#include "soc_osal.h"
#include "securec.h"
#include "osal_debug.h"
#include "cmsis_os2.h"
#include "bsp/io_mode_init.h"
#include "bsp/hal_bsp_sgp30.h"
#include "bsp/hal_bsp_ssd1306.h"
#include "bsp/hal_bsp_pcf8574.h"
#include "app_init.h"
#include "osal_task.h"

osThreadId_t Task1_ID; // 任务1设置为低优先级任务
osThreadId_t Task2_ID;
osThreadId_t Task3_ID;

#define SEC_MAX 60
#define MIN_MAX 60
#define HOUR_MAX 24
void my_peripheral_init(void);
#define TASK_DELAY_TIME 1000
uint8_t adcchannel = 5;
/* 主任务 */
void main_task(void *argument)
{
    unused(argument);
    while (1) {
        // 显示数据
        oled_show();
#if (DEIVER_BOARD)
        // 读取io拓展芯片引脚状态
        PCF8574_Read(&systemdata.pcf8574_int_io);
#endif
        SGP30_ReadData(&systemdata.CO2, &systemdata.TVOC); // 获取CO2和TVOC数据
        printf("%d  %d  %x\n", systemdata.CO2, systemdata.TVOC, systemdata.pcf8574_int_io);
        uapi_gpio_toggle(GPIO_13);
        osal_msleep(500);
    }
}
/* 电机控制任务 */
void motor_control_task(void *argument)
{
    unused(argument);
    while (1) {

#if (DEIVER_BOARD)         // 如果定义了 DEIVER_BOARD 宏，使用 PCF8574 芯片控制电机
        if (pwm_flag == 1) // 如果 pwm_flag 为 1，表示电机正转
        {
            tmp_io.bit.p2 = 1;
            tmp_io.bit.p6 = 0;
            PCF8574_Write(tmp_io.all);
        } else if (pwm_flag == 2) // 如果 pwm_flag 为 2，表示电机反转
        {
            tmp_io.bit.p2 = 0;
            tmp_io.bit.p6 = 1;
            PCF8574_Write(tmp_io.all);
        } else if (pwm_flag == 3) // 如果 pwm_flag 为 3，表示电机停
        {
            tmp_io.bit.p2 = 0;
            tmp_io.bit.p6 = 0;
            PCF8574_Write(tmp_io.all);
            pwm_flag = 0;
        }

#else // 如果未定义 DEIVER_BOARD 宏，使用 PWM 通道控制电机
        if (pwm_flag == 1) {
            uapi_pwm_open(PWM1_CHANNEL, &PWM_HIGH); // 打开 PWM1 通道并设置为高电平
            uapi_pwm_open(PWM0_CHANNEL, &PWM_LOW);  // 打开 PWM0 通道并设置为低电平
            uapi_pwm_start(PWM1_CHANNEL);           // 启动 PWM1 通道
            uapi_pwm_start(PWM0_CHANNEL);           // 启动 PWM0 通道
        } else if (pwm_flag == 2) {
            uapi_pwm_open(PWM1_CHANNEL, &PWM_LOW);
            uapi_pwm_open(PWM0_CHANNEL, &PWM_HIGH);
            uapi_pwm_start(PWM1_CHANNEL);
            uapi_pwm_start(PWM0_CHANNEL);
        } else if (pwm_flag == 3) {
            uapi_pwm_open(PWM1_CHANNEL, &PWM_LOW);
            uapi_pwm_open(PWM0_CHANNEL, &PWM_LOW);
            uapi_pwm_start(PWM1_CHANNEL);
            uapi_pwm_start(PWM0_CHANNEL);
            pwm_flag = 0;
        }
#endif
        osal_msleep(100);
    }
}

/* IO读取 */
void get_sensor_io_task(void *argument)
{
    unused(argument);
    while (1) {
#if (DEIVER_BOARD) // 如果定义了 DEIVER_BOARD 宏，从 PCF8574 芯片读取传感器状态

        if (systemdata.pcf8574_int_io & 0x10) { // 如果 PCF8574 的引脚状态的第 4 位为 1，表示火焰传感器关闭
            systemdata.flame_status = SENSOR_OFF;
        } else {
            systemdata.flame_status = SENSOR_ON;
        }
        if (systemdata.pcf8574_int_io & 0x80) { // 如果 PCF8574 的引脚状态的第 7 位为 1，表示气体传感器关闭
            systemdata.gas_status = SENSOR_OFF;
        } else {
            systemdata.gas_status = SENSOR_ON;
        }

#else // 如果未定义 DEIVER_BOARD 宏，直接读取  ADC 数据
        if (uapi_gpio_get_val(USER_FLAME) == 1) { // 如果火焰传感器对应的 GPIO 引脚为高电平，表示火焰传感器关闭
            systemdata.flame_status = SENSOR_OFF;
        } else {
            systemdata.flame_status = SENSOR_ON;
        }
        adc_port_read(adcchannel, &systemdata.GAS_MIC); // 调用 adc_port_read 函数读取气体传感器的 ADC 数据

#endif
        osal_msleep(400);
    }
}
/* 外设初始化 */
void my_peripheral_init(void)
{
    user_led_init();
    SSD1306_Init(); // OLED 显示屏初始化
    SSD1306_CLS();  // 清屏
    SGP30_Init();
#if (DEIVER_BOARD) // 如果定义了 DEIVER_BOARD 宏，初始化 PCF8574 芯片
    PCF8574_Init();
#else // 如果未定义 DEIVER_BOARD 宏，初始化 PWM 和传感器 IO
    user_pwm_init();
    sensor_io_init();
#endif

    user_key_init();
}
static void smart_agriculture_demo(void)
{
    printf("Enter smart_agriculture_demo()!\r\n");
    my_peripheral_init();
    osThreadAttr_t attr;
    attr.name = "motor_control_task";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 0x2000;
    attr.priority = osPriorityNormal;

    Task1_ID = osThreadNew((osThreadFunc_t)main_task, NULL, &attr);
    if (Task1_ID != NULL) {
        printf("ID = %d, Create Task1_ID is OK!\r\n", Task1_ID);
    }
    attr.stack_size = 0x1000;
    Task2_ID = osThreadNew((osThreadFunc_t)motor_control_task, NULL, &attr);
    if (Task2_ID != NULL) {
        printf("ID = %d, Create Task1_ID is OK!\r\n", Task2_ID);
    }
    Task3_ID = osThreadNew((osThreadFunc_t)get_sensor_io_task, NULL, &attr);
    if (Task3_ID != NULL) {
        printf("ID = %d, Create Task1_ID is OK!\r\n", Task3_ID);
    }
}
app_run(smart_agriculture_demo);