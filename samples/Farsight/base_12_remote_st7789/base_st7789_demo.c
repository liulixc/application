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

#include "soc_osal.h"
#include "securec.h"
#include "osal_debug.h"
#include "cmsis_os2.h"
#include "hal_bsp_oled/bsp_st7789_4line.h"
#include "app_init.h"

osThreadId_t Task1_ID; // 任务1设置为低优先级任务

void Task1(void)
{
    printf("oled_init!\r\n");
    // 初始化 spi 引脚及配置
    app_spi_init_pin();
    app_spi_master_init_config();
    // 屏幕初始化
    ST7789_Init();
    // 刷新屏幕颜色 白色
    ST7789_Clear(WHITE);
    osal_msleep(2000);
    ST7789_Clear(YELLOW);
    osal_msleep(2000);
    ST7789_Clear(BLACK);
    osal_msleep(2000);
    // 显示英文字符
    DisplayString_chat(0, 0, WHITE, "ABCDEFGHIJKLMNOPQRSTUVWXYZ!");
    while (1) {
        osal_msleep(2000);
    }
}
static void base_st7789_demo(void)
{
    printf("Enter base_st7789_demo()!\r\n");

    osThreadAttr_t attr;
    attr.name = "Task1";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 0x2000;
    attr.priority = osPriorityNormal;

    Task1_ID = osThreadNew((osThreadFunc_t)Task1, NULL, &attr);
    if (Task1_ID != NULL) {
        printf("ID = %d, Create Task1_ID is OK!\r\n", Task1_ID);
    }
}
app_run(base_st7789_demo);