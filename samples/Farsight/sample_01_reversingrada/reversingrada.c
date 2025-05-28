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
#include "bsp/hal_bsp_pcf8574.h"
#include "bsp/hal_bsp_bmps.h"
#include "app_init.h"
#include "osal_task.h"
#include "watchdog.h"
osThreadId_t Task1_ID;

void main_task(void *argument)
{
    unused(argument);
    while (1) {
        // 检查变量 distance 的值是否小于 500
        if (distance < 500) {
            // 如果 distance 小于 500， 函数翻转引脚的电平状态 使蜂鸣器开关
            uapi_gpio_toggle(USER_BEEP);
            uapi_gpio_toggle(USER_LED);
            osal_mdelay(300);
        } else {
            // 如果 distance 大于等于 500，将引脚电平设置为低电平
            uapi_gpio_set_val(USER_LED, GPIO_LEVEL_LOW);
            uapi_gpio_set_val(USER_BEEP, GPIO_LEVEL_LOW);
        }
        // 显示距离
        show_page(distance);
    }
}
/* 外设初始化 */
void my_peripheral_init(void)
{
    user_led_init();
    user_beep_init();
    SSD1306_Init(); // OLED 显示屏初始化
    SSD1306_CLS();  // 清屏
    PCF8574_Init();
    uapi_watchdog_disable();
    app_uart_init_config();
}
static void reversingrada_demo(void)
{
    printf("Enter reversingrada_demo()!\r\n");
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
}
app_run(reversingrada_demo);
