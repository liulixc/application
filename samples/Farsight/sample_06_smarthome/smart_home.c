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
#include "bsp/bsp_init.h"
#include "bsp/hal_bsp_nfc.h"
#include "bsp/hal_bsp_nfc_to_wifi.h"
#include "bsp/hal_bsp_ap3216.h"
#include "bsp/hal_bsp_bmps.h"
#include "bsp/wifi_connect.h"
#include "bsp/mqtt.h"
#include "bsp/hal_bsp_sht20.h"
#include "app_init.h"
#include "osal_task.h"
#include "watchdog.h"
osThreadId_t Task1_ID;
osThreadId_t Task2_ID;
osThreadId_t Task3_ID;
osThreadId_t Task4_ID;

char g_response_buf[] =
    "{\"result_code\": 0,\"response_name\": \"beep\",\"paras\": {\"result\": \"success\"}}"; // 响应json

char g_buffer[512] = {0}; // 发布数据缓冲区

uint8_t display_flag;

void display_task(void)
{
    // 等待 MQTT 连接成功，若未连接则每隔 100 个时间单位检查一次
    while (!mqtt_conn) {
        osDelay(100);
    }
    // 设置智能控制标志为 1，表示开启智能控制模式
    sys_msg_data.smartControl_flag = 1;
    // 在 LCD 上指定位置绘制图标
    LCD_DrawPicture(20, 0, 64, 44, (uint8_t *)gImage_auto);
    LCD_DrawPicture(85, 0, 234, 44, (uint8_t *)title);

    // 根据 MQTT 连接状态绘制相应的图片
    if (mqtt_conn == 1) {
        LCD_DrawPicture(296, 0, 320, 24, (uint8_t *)conn_wifi);
    } else {
        LCD_DrawPicture(296, 0, 320, 24, (uint8_t *)no_wifi);
    }
    LCD_DrawPicture(0, 44, 320, 47, (uint8_t *)blackline);

    LCD_DrawPicture(0, 61, 68, 129, (uint8_t *)gImage_temp);
    LCD_DrawPicture(0, 143, 68, 211, (uint8_t *)gImage_fanoff);

    LCD_DrawPicture(106, 61, 174, 129, (uint8_t *)gImage_rentioff);
    LCD_DrawPicture(106, 143, 174, 211, (uint8_t *)gImage_beepoff);

    LCD_DrawPicture(212, 61, 280, 129, (uint8_t *)gImage_lampoff);
    LCD_DrawPicture(212, 143, 280, 211, (uint8_t *)gImage_keyoff);
    // 设置显示标志为 1，表示显示初始化完成
    display_flag = 1;
    while (1) {
        // 调用 lcd_display 函数进行 LCD 显示更新
        lcd_display();

        osal_mdelay(100);
    }
}

/**
 * @brief  传感器采集任务
 * @note
 * @retval None
 */
void sensor_collect_task(void *argument)
{
    unused(argument);
    // 等待显示任务初始化完成，若未完成则每隔 100 个时间单位检查一次
    while (!display_flag) {
        osDelay(100);
    }

    while (1) {
        // 若有触摸事件，则进行触摸扫描并重置计数
        if (ft6336_on_touch_count) {
            FT6336_scan();
            ft6336_on_touch_count = 0;
        }
        // 调用 SHT20_ReadData 函数读取温度和湿度数据
        SHT20_ReadData(&sys_msg_data.temperature, &sys_msg_data.humidity);
        // 调用 AP3216C_ReadData 函数读取红外、环境光和接近感应数据
        AP3216C_ReadData(&sys_msg_data.irData, &sys_msg_data.alsData, &sys_msg_data.psData);
        // 根据环境数据与阈值比较结果设置外设状态
        sys_msg_data.fanStatus = (sys_msg_data.temperature > sys_msg_data.threshold_value.temp_threshold_value ? 1 : 0);
        sys_msg_data.is_Body = (sys_msg_data.psData > sys_msg_data.threshold_value.ps_threshold_value ? 1 : 0);
        sys_msg_data.lamp_state = (sys_msg_data.alsData < sys_msg_data.threshold_value.als_threshold_value ? 1 : 0);
        if (sys_msg_data.smartControl_flag) { // 自动控制模式
            // 检测到有人，开启蜂鸣器
            if (sys_msg_data.is_Body) {
                uapi_gpio_set_val(USER_BEEP, 1);
                sys_msg_data.beep_status = 1;
            } else {
                uapi_gpio_set_val(USER_BEEP, 0);
                sys_msg_data.beep_status = 0;
            }

            // 当光照强度小于阈值，开启LED灯
            if (sys_msg_data.lamp_state)
                uapi_gpio_set_val(USER_LED, 0);
            else
                uapi_gpio_set_val(USER_LED, 1);
        } else {
            // 开启任务锁，防止线程共用资源
            osKernelLock();
            // 按键按下后，改变按键状态，并控制风扇
            if (sys_msg_data.touch_key) {
                LCD_DrawPicture(212, 143, 280, 211, (uint8_t *)gImage_keyon);
                sys_msg_data.fanStatus = 1;
            } else {
                LCD_DrawPicture(212, 143, 280, 211, (uint8_t *)gImage_keyoff);
                sys_msg_data.fanStatus = 0;
            }
            osKernelUnlock();
        }

        osal_mdelay(100);
    }
}

void main_task(void *argument)
{
    unused(argument);
    user_led_init();
    user_beep_init();
    app_spi_init_pin();
    app_spi_master_init_config();
    ili9341_init();
    osDelay(50);
    FT6336_init();
    AP3216C_Init();
    // 建立 WiFi 连接
    nfc_connect_wifi_init();
    osal_mdelay(1000);
    // 建立 MQTT 连接
    mqtt_connect();

    while (1) {
        osal_mdelay(1000);
    }
}

void mqtt_report_task(void *argument)
{
    unused(argument);
    while (1) { // 响应平台命令部分
        if (g_cmdFlag) {
            sprintf(g_buffer, MQTT_CLIENT_RESPONSE, g_response_id);
            // 设备响应命令
            mqtt_publish(g_buffer, g_response_buf);
            g_cmdFlag = 0;
            memset(g_response_id, 0, sizeof(g_response_id) / sizeof(g_response_id[0]));
        }
        osDelay(20);
    }
}
/* 外设初始化 */
void my_peripheral_init(void)
{
    KEY_Init();
    sys_msg_data.threshold_value.als_threshold_value = 300;
    sys_msg_data.threshold_value.ps_threshold_value = 200;
    sys_msg_data.threshold_value.temp_threshold_value = 30.0;
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
    if (Task2_ID != NULL) {
        printf("ID = %d, Create Task2_ID is OK!\r\n", Task2_ID);
    }
    Task3_ID = osThreadNew((osThreadFunc_t)mqtt_report_task, NULL, &attr);
    if (Task3_ID != NULL) {
        printf("ID = %d, Create Task3_ID is OK!\r\n", Task3_ID);
    }
    Task4_ID = osThreadNew((osThreadFunc_t)display_task, NULL, &attr);
    if (Task4_ID != NULL) {
        printf("ID = %d, Create Task4_ID is OK!\r\n", Task4_ID);
    }
}
app_run(smart_farm_demo);
