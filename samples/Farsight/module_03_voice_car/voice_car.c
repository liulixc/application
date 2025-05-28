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
#include "bsp/hal_bsp_ssd1306.h"
#include "bsp/bsp_init.h"
#include "bsp/hal_bsp_dc.h"
#include "bsp/bsp_ina219.h"
#include "bsp/hal_bsp_pcf8574.h"
#include "app_init.h"
#include "osal_task.h"
#include "watchdog.h"
osThreadId_t Task1_ID; // 任务1设置为低优先级任务
osThreadId_t Task2_ID; // 任务1设置为低优先级任务
osThreadId_t Task3_ID; // 任务1设置为低优先级任务

uint8_t car_mode = 0;
void main_task(void *argument)
{
    unused(argument);
    while (1) {
        // 从 SHT20 传感器读取温湿度数据 将读取到的温度值存储到 temperature 变量，湿度值存储到 humidity 变量
        SHT20_ReadData(&temperature, &humidity);
#if (DEIVER_BOARD) // 如果定义了 DEIVER_BOARD 则使用驱动板，可以获取电池电压
        // 从 INA219 传感器获取系统电池的总线电压
        INA219_get_bus_voltage_mv(&systemValue.battery_voltage);
#endif
        // 显示传感器数据
        oled_show();
        uapi_gpio_toggle(GPIO_13);
        osal_msleep(300);
    }
}
// 接受模组串口数据
void recv_task(void *argument)
{
    unused(argument);
    while (1) {
        if (uart2_recv.recv_flag) {
            recv_uart_hex();
        }
        osal_msleep(2);
    }
}
/* 外设初始化 */
void my_peripheral_init(void)
{
    user_led_init();
    user_beep_init();
    SSD1306_Init(); // OLED 显示屏初始化
    SSD1306_CLS();  // 清屏
#if (DEIVER_BOARD)
    PCF8574_Init();
    INA219_Init();
#endif

    uapi_watchdog_disable(); // 关闭看门狗
    user_pwm_init();         // pwm外设初始化
    car_io_init();           // 小车状态初始化
    app_uart_init_config();  // 串口初始化
}
static void voicecar_demo(void)
{
    printf("Enter voicecar_demo()!\r\n");
    my_peripheral_init();
    osThreadAttr_t attr;
    attr.name = "Task1";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 0x1000;
    attr.priority = osPriorityNormal;

    Task1_ID = osThreadNew((osThreadFunc_t)main_task, NULL, &attr);
    if (Task1_ID != NULL) {
        printf("ID = %d, Create Task1_ID is OK!\r\n", Task1_ID);
    }
    Task2_ID = osThreadNew((osThreadFunc_t)recv_task, NULL, &attr);
    if (Task1_ID != NULL) {
        printf("ID = %d, Create Task2_ID is OK!\r\n", Task2_ID);
    }
}
app_run(voicecar_demo);
