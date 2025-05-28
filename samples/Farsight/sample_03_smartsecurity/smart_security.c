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
#include "bsp/hal_bsp_nfc.h"
#include "bsp/hal_bsp_nfc_to_wifi.h"
#include "bsp/hal_bsp_bmps.h"
#include "bsp/wifi_connect.h"
#include "bsp/mqtt.h"
#include "bsp/hal_bsp_ap3216.h"
#include "app_init.h"
#include "osal_task.h"
#include "watchdog.h"
osThreadId_t Task1_ID;
osThreadId_t Task2_ID;
#define PS_SENSOR_MAX 100 // 接近传感器(PS)的检测值
char g_response_buf[] =
    "{\"result_code\": 0,\"response_name\": \"beep\",\"paras\": {\"result\": \"success\"}}"; // 响应json

char g_buffer[512] = {0}; // 发布数据缓冲区
/**
 * @brief  传感器采集任务
 * @note
 * @retval None
 */
void sensor_collect_task(void *argument)
{
    unused(argument);
    while (!mqtt_conn) {
        osDelay(100);
    }
    AP3216C_Init();
    while (1) {
        // 采集传感器的值
        uint16_t irData, alsData;
        AP3216C_ReadData(&irData, &alsData, &sys_msg_data.psData);
        // 逻辑判断 当接近传感器检测有物体接近的时候，表示有人靠近
        sys_msg_data.is_Body = (sys_msg_data.psData > PS_SENSOR_MAX ? 1 : 0);
        printf("%d %d\r\n", sys_msg_data.is_Body, sys_msg_data.smartControl_flag);
        // 显示在OLED显示屏上
        show_page();
        // 是否开启自动控制
        if (sys_msg_data.smartControl_flag != 0) {
            if (sys_msg_data.is_Body != 0) {
                // 开启蜂鸣器报警
                set_buzzer(1);
            } else {
                // 关闭蜂鸣器报警
                set_buzzer(0);
            }
        }
        // 响应平台命令部分
        if (g_cmdFlag) {
            sprintf(g_buffer, MQTT_CLIENT_RESPONSE, g_response_id);
            // 设备响应命令
            mqtt_publish(g_buffer, g_response_buf);
            g_cmdFlag = 0;
            memset(g_response_id, 0, sizeof(g_response_id) / sizeof(g_response_id[0]));
        }
        sys_msg_data.buzzerStatus = uapi_gpio_get_val(USER_BEEP);
        osDelay(10);
    }
}

void main_task(void *argument)
{
    unused(argument);
    if (nfc_connect_wifi_init() == -1) {
        return;
    }
    mqtt_connect();
}
/* 外设初始化 */
void my_peripheral_init(void)
{
    user_led_init();
    user_beep_init();
    SSD1306_Init(); // OLED 显示屏初始化
    SSD1306_CLS();  // 清屏

    uapi_watchdog_disable();
}
static void smart_farm_demo(void)
{
    printf("Enter smart_farm_demo()!\r\n");
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
    Task2_ID = osThreadNew((osThreadFunc_t)sensor_collect_task, NULL, &attr);
    if (Task1_ID != NULL) {
        printf("ID = %d, Create Task2_ID is OK!\r\n", Task2_ID);
    }
}
app_run(smart_farm_demo);
