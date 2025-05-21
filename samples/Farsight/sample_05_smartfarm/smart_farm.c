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
#include "bsp/oled_show_log.h"
#include "bsp/hal_bsp_sht20.h"
#include "app_init.h"
#include "osal_task.h"
#include "watchdog.h"
osThreadId_t Task1_ID;
osThreadId_t Task2_ID;
osThreadId_t Task3_ID;
#define TASK_DELAY_TIME (100 * 1000) // us
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
    float temperature = 0, humidity = 0;
    // 等待 MQTT 连接成功，若未连接则每隔 100 个时间单位检查一次
    while (!mqtt_conn) {
        osDelay(100);
    }

    while (1) {
        // 采集传感器的值
        SHT20_ReadData(&temperature, &humidity);
        show_humi_page(humidity);

        sys_msg_data.temperature = temperature;
        sys_msg_data.humidity = humidity;

        // 查看是否开启自动控制
        if (sys_msg_data.threshold_value.smartControl_flag != 0) {
            // 如果当前湿度大于等于湿度上限值
            if (sys_msg_data.humidity >= sys_msg_data.threshold_value.humi_upper) {
                set_fan(1); // 风扇打开
                sys_msg_data.fanStatus = 1;
            } else if (sys_msg_data.humidity <= sys_msg_data.threshold_value.humi_lower) {
                set_fan(0); // 风扇关闭
                sys_msg_data.fanStatus = 0;
            } else {
                // 保持上一状态; 上一次状态是开，那就继续开; 反之，关
                set_fan(sys_msg_data.fanStatus);
            }
        } else {
            if (sys_msg_data.fanStatus != 0) {
                set_fan(1); // 风扇打开
            } else {
                set_fan(0); // 风扇关闭
            }
        }

        osal_udelay(TASK_DELAY_TIME);
    }
}

void main_task(void *argument)
{
    unused(argument);
    // 调用 nfc_connect_wifi_init 函数进行 NFC 连接 Wi-Fi 初始化
    if (nfc_connect_wifi_init() == -1) {
        return;
    }
    // 调用 mqtt_connect 函数进行 MQTT 连接
    mqtt_connect();
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
    user_fan_init();
    KEY_Init();
    SSD1306_Init(); // OLED 显示屏初始化
    SSD1306_CLS();  // 清屏
    sys_msg_data.threshold_value.humi_lower = 22;
    sys_msg_data.threshold_value.humi_upper = 70;
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
    Task3_ID = osThreadNew((osThreadFunc_t)mqtt_report_task, NULL, &attr);
    if (Task1_ID != NULL) {
        printf("ID = %d, Create Task3_ID is OK!\r\n", Task3_ID);
    }
}
app_run(smart_farm_demo);
