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
#include "common_def.h"
#include "stdio.h"
#include "string.h"
#include "soc_osal.h"
#include "securec.h"
#include "osal_debug.h"
#include "cmsis_os2.h"
#include "bsp/hal_bsp_ssd1306.h"
#include "app_init.h"
#include "osal_task.h"
#include "watchdog.h"
#include "bsp/hal_bsp_bmps.h"
#include "bsp/hal_bsp_sht20.h"
osThreadId_t Task1_ID;
#define TASK_DELAY_TIME 2000
void main_task(void *argument)
{
    unused(argument);
    float temp_val, humi_val;
    while (1) {
        SHT20_ReadData(&temp_val, &humi_val); // 读取温湿度传感器的值
        show_temp_page(temp_val); // 显示温度页面
        osal_mdelay(TASK_DELAY_TIME);
        show_humi_page(humi_val); // 显示湿度页面
        osal_mdelay(TASK_DELAY_TIME);
    }
}
/* 外设初始化 */
void my_peripheral_init(void)
{
    SSD1306_Init(); // OLED 显示屏初始化
    SSD1306_CLS();  // 清屏
    SHT20_Init();
    uapi_watchdog_disable();
}
static void smarttemp_demo(void)
{
    printf("Enter smarttemp_demo()!\r\n");
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
app_run(smarttemp_demo);
