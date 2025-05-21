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
#include "bsp/hal_bsp_ap3216.h"
#include "bsp/hal_bsp_aw2013.h"
#include "app_init.h"
#include "osal_task.h"
#include "watchdog.h"
osThreadId_t Task1_ID;
osThreadId_t Task2_ID;
osThreadId_t Task3_ID;
#define TASK_DELAY_TIME (1000) // us
#define KEY_COUNT_TIEMS (10)
#define SENSOR_COUNT_TIMES (50)
#define CHANGE_MODE_COUNT_TIMES (100)

char oled_display_buff[30] = {0};
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
    uint16_t times = 0;
    uint8_t last_mode = sys_msg_data.Lamp_Status;
    // 当 MQTT 未连接时，任务进入循环等待
    while (!mqtt_conn) {
        osDelay(100);
    }
    // 初始化 AP3216C 传感器，为后续读取传感器数据做准备
    AP3216C_Init();
    // 初始化 AW2013 芯片，该芯片可能用于控制 RGB 灯
    AW2013_Init();
    // 调用 AW2013_Control_RGB 函数将 RGB 灯的颜色设置为全黑
    AW2013_Control_RGB(0, 0, 0);
    while (1) {
        // 检测按键的值 如果当前处于手动灯光模式
        if (sys_msg_data.is_auto_light_mode == LIGHT_MANUAL_MODE) {
            // 检查当前灯的状态是否与上一次记录的状态不同
            if (last_mode != sys_msg_data.Lamp_Status) {
                // 如果状态不同，调用 switch_rgb_mode 函数切换 RGB 灯的工作模式
                switch_rgb_mode(sys_msg_data.Lamp_Status);
            }
            // 更新 last_mode 为当前灯的状态
            last_mode = sys_msg_data.Lamp_Status;
        }

        // 采集传感器的值
        if (!(times % SENSOR_COUNT_TIMES)) {
            AP3216C_ReadData(&sys_msg_data.AP3216C_Value.infrared, &sys_msg_data.AP3216C_Value.proximity,
                             &sys_msg_data.AP3216C_Value.light);
            // 如果当前处于手动灯光模式
            if (sys_msg_data.is_auto_light_mode == LIGHT_MANUAL_MODE) {
                memset(oled_display_buff, 0, sizeof(oled_display_buff));
                sprintf(oled_display_buff, "light:%04d", sys_msg_data.AP3216C_Value.light);
                SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_1, oled_display_buff, TEXT_SIZE_16);
                memset(oled_display_buff, 0, sizeof(oled_display_buff));
                sprintf(oled_display_buff, "Lamp:%s", (sys_msg_data.Lamp_Status == OFF_LAMP) ? "OFF" : " ON");
                SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_2, oled_display_buff, TEXT_SIZE_16);
            }
            // 将 times 重置为 0，以便下一次计数
            times = 0;
        }
        // 更改LED灯的工作模式
        if (!(times % CHANGE_MODE_COUNT_TIMES)) {
            memset(oled_display_buff, 0, sizeof(oled_display_buff));
            sprintf(oled_display_buff, "auto control:%s", (sys_msg_data.is_auto_light_mode == 1) ? " ON" : "OFF");
            SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_0, oled_display_buff, TEXT_SIZE_16);
            switch_rgb_mode(sys_msg_data.Lamp_Status);
        }
        times++;
        osal_udelay(TASK_DELAY_TIME);
    }
}

void main_task(void *argument)
{
    unused(argument);
    // 判断nfc配网是否成功
    if (nfc_connect_wifi_init() == -1) {
        return;
    }
    // 如果成功 则连接mqtt服务器
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
    user_led_init();
    KEY_Init();
    SSD1306_Init();                     // OLED 显示屏初始化
    SSD1306_CLS();                      // 清屏
    sys_msg_data.led_light_value = 100; // RGB灯的亮度值为100%的状态
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
