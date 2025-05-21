/*
 * Copyright (c) 2024 Beijing HuaQingYuanJian Education Technology Co., Ltd.
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
#include "cmsis_os2.h"
#include "hal_bsp_lcd/bsp_ili9341_4line.h"
#include "hal_bsp_lcd/bsp_ft6336.h"
#include "app_init.h"
osThreadId_t task1_ID; // 任务1设置为低优先级任务

#define TASK_DELAY_TIME 100
char str[] = "1234567890*#&| ABCD abcd !";
void Task1(void)
{
    osDelay(TASK_DELAY_TIME);
    // 对 SPI 进行初始化
    app_spi_init_pin();
    app_spi_master_init_config();
    printf("Enter ILI9341_Init()!\r\n");
    // 对 ILI9341 显示驱动芯片进行初始化
    ILI9341_Init();
    osDelay(100);
    // 对 FT6336 触摸驱动芯片进行初始化
    FT6336_init();
    while (1) {
        // 如果定义了 TOUCH_INT 宏，表示使用触摸中断模式
#if (TOUCH_INT)
        osDelay(100);
#else // 如果未定义 TOUCH_INT 宏，使用轮询模式
      // FT6336 触摸点读取
        FT6336_scan();
        osDelay(10);
#endif
    }
}

static void base_lcd_demo(void)
{
    printf("Enter base_lcd_demo()!\r\n");

    osThreadAttr_t attr;
    attr.name = "Task1";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 0x3000;
    attr.priority = osPriorityNormal;

    task1_ID = osThreadNew((osThreadFunc_t)Task1, NULL, &attr);
    if (task1_ID != NULL) {
        printf("ID = %d, Create task1_ID is OK!\r\n", task1_ID);
    }
}
app_run(base_lcd_demo);