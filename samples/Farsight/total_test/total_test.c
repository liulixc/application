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
#include "bsp/hal_bsp_aw2013.h"
#include "bsp/hal_bsp_ap3216.h"
#include "bsp/bsp_ft6336.h"
#include "bsp/bsp_ili9341_4line.h"
#include "bsp/hal_bsp_nfc.h"
#include "bsp/bsp_init.h"
#include "app_init.h"
#include "osal_task.h"
#include "watchdog.h"
osThreadId_t Task1_ID; // 任务1设置为低优先级任务
osThreadId_t Task2_ID; // 任务1设置为低优先级任务
osThreadId_t Task3_ID; // 任务1设置为低优先级任务
uint16_t ir, als, ps;  // 人体红外传感器 接近传感器 光照强度传感器
extern char str[30];

void nfc_read(void)
{
    uint8_t ndefLen = 0;     // ndef包的长度
    uint8_t ndef_Header = 0; // ndef消息开始标志位-用不到
    size_t i = 0;
    // 读整个数据的包头部分，读出整个数据的长度
    if (NT3HReadHeaderNfc(&ndefLen, &ndef_Header) != true) {
        printf("NT3HReadHeaderNfc is failed.\r\n");
        return;
    }

    ndefLen += NDEF_HEADER_SIZE; // 加上头部字节
    if (ndefLen <= NDEF_HEADER_SIZE) {
        printf("ndefLen <= 2\r\n");
        return;
    }
    uint8_t *ndefBuff = (uint8_t *)malloc(ndefLen + 1);
    if (ndefBuff == NULL) {
        printf("ndefBuff malloc is Falied!\r\n");
        return;
    }

    if (get_NDEFDataPackage(ndefBuff, ndefLen) != ERRCODE_SUCC) {
        printf("get_NDEFDataPackage is failed. \r\n");
        return;
    }
    nfc_test = 1;
    printf("start print ndefBuff.\r\n");
    for (i = 0; i < ndefLen; i++) {
        printf("0x%x ", ndefBuff[i]);
    }
}

void main_task(void *argument)
{
    unused(argument);
    osal_msleep(500);
    nfc_read();
    AP3216C_Init();
    AW2013_Init(); // 三色LED灯的初始化
    AW2013_Control_RGB(0xff, 0, 0);
    osal_msleep(200);
    AW2013_Control_RGB(0, 0xff, 0);
    osal_msleep(200);
    AW2013_Control_RGB(0, 0, 0xff);
    uapi_gpio_set_val(USER_BEEP, GPIO_LEVEL_HIGH);
    uapi_gpio_set_val(USER_LED, GPIO_LEVEL_HIGH);
    osal_msleep(200);
    uapi_gpio_set_val(USER_BEEP, GPIO_LEVEL_LOW);
    AW2013_Control_RGB(0, 0, 0);
    app_spi_init_pin();
    app_spi_master_init_config();
    ILI9341_Init();
    LCD_ShowString(60, 0, strlen("TOUCH TEST"), 24, (uint8_t *)"TOUCH TEST");
    osDelay(50);
    FT6336_init();
    while (1) {
        AP3216C_ReadData(&ir, &als, &ps);
        SHT20_ReadData(&temperature, &humidity);
        oled_show();
        osDelay(100);
    }
}

/* 外设初始化 */
void my_peripheral_init(void)
{
    user_led_init();
    user_beep_init();
    SSD1306_Init(); // OLED 显示屏初始化
    SSD1306_CLS();  // 清屏
    user_key_init();
    uapi_watchdog_disable();
}

static void voice_demo(void)
{
    printf("Enter voice_demo()!\r\n");
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
app_run(voice_demo);
