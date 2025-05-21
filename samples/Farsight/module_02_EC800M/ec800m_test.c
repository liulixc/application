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
#include "bsp/hal_bsp_ec800m.h"
#include "bsp/bsp_ina219.h"
#include "bsp/hal_bsp_pcf8574.h"
#include "app_init.h"
#include "osal_task.h"
#include "watchdog.h"
osThreadId_t Task1_ID;
osThreadId_t Task2_ID;
osThreadId_t Task3_ID;
osThreadId_t Task4_ID;
char ec800m_sendcmd[255] = {0};
char ec800m1_sendcmd[255] = {0};

void main_task(void *argument)
{
    unused(argument);
    while (1) {
        // 从 SHT20 传感器读取温湿度数据 将读取到的温度值存储到 temperature 变量，湿度值存储到 humidity 变量
        SHT20_ReadData(&temperature, &humidity);
        // 从 INA219 传感器获取系统电池的总线电压
        INA219_get_bus_voltage_mv(&systemValue.battery_voltage);
        // 该函数可能负责在 OLED 显示屏上显示信息
        oled_show();
        uapi_gpio_toggle(GPIO_13);
        osal_msleep(300);
    }
}
void execute_carcommand(void *argument)
{
    unused(argument);
    // 将 car_status 设置为 CAR_STATUS_STOP，表示汽车处于停止状态
    systemValue.car_status = CAR_STATUS_STOP;
    while (1) {
        // 传入 systemValue 结构体，控制小车的状态
        control_data(systemValue);
        osal_msleep(100);
    }
}
void uart2_recv_task(void *argument)
{
    unused(argument);
    uint8_t len = 0;
    uint16_t times = 0;
    while (1) {

        if (uart2_recv.recv_flag) {
            if (times % 100 == 0) {
                printf("获取GPS数据\n");
                uart_send_buff((uint8_t *)ATCmds[17].ATSendStr, strlen(ATCmds[17].ATSendStr));
            }
            len = uapi_uart_read(UART_ID, uart2_recv.recv, 512, 0);
            if (len > 0) {
                printf("[uart2_recv.recv %d: %s]\n", len, uart2_recv.recv);
                get_gps_coordinates((char *)uart2_recv.recv);
                get_car_cmd((char *)uart2_recv.recv);
                memset(uart2_recv.recv, 0, sizeof(uart2_recv.recv));
            }
        }
        osal_msleep(100);
        times++;
    }
}

void uart2_send_task(void *argument)
{
    unused(argument);
    ec800_pwr_init();
    ec800_send_init();
    uart2_recv.recv_flag = 1;
    sprintf(ec800m_sendcmd, ATCmds[14].ATSendStr, MQTT_USER_NAME);
    while (1) {
        sprintf(ec800m1_sendcmd, ATCmds[15].ATSendStr, (int)temperature, (int)humidity,
                ((float)(systemValue.battery_voltage) / COEFFICIENT_1000), get_CurrentCarStatus(systemValue), latitude,
                longitude);
        uart_send_buff((uint8_t *)ec800m_sendcmd, strlen(ec800m_sendcmd));
        osal_msleep(100);
        uart_send_buff((uint8_t *)ec800m1_sendcmd, strlen(ec800m1_sendcmd));
        osal_msleep(100);
        uart_send_buff((uint8_t *)ATCmds[16].ATSendStr, strlen(ATCmds[16].ATSendStr));
        osal_msleep(2000);
    }
}
/* 外设初始化 */
void my_peripheral_init(void)
{
    user_led_init();
    SSD1306_Init(); // OLED 显示屏初始化
    SSD1306_CLS();  // 清屏
    PCF8574_Init();
    car_io_init();
    uapi_watchdog_disable();
    user_pwm_init();
    app_uart_init_config();
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
    Task2_ID = osThreadNew((osThreadFunc_t)uart2_send_task, NULL, &attr);
    if (Task1_ID != NULL) {
        printf("ID = %d, Create Task2_ID is OK!\r\n", Task2_ID);
    }

    Task3_ID = osThreadNew((osThreadFunc_t)execute_carcommand, NULL, &attr);
    if (Task1_ID != NULL) {
        printf("ID = %d, Create Task2_ID is OK!\r\n", Task3_ID);
    }
    attr.priority = osPriorityNormal1;
    Task4_ID = osThreadNew((osThreadFunc_t)uart2_recv_task, NULL, &attr);
    if (Task1_ID != NULL) {
        printf("ID = %d, Create Task2_ID is OK!\r\n", Task4_ID);
    }
}
app_run(voicecar_demo);
