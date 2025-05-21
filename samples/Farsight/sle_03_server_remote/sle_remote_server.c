/*
 * Copyright (c) 2024 HiSilicon Technologies CO., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "common_def.h"
#include "soc_osal.h"
#include "securec.h"
#include "bsp/bsp_ina219.h"
#include "bsp/bsp_init.h"
#include "bsp/sle_server.h"
#include "bsp/hal_bsp_ssd1306.h"
#include "bsp/hal_bsp_dc.h"
#include "bsp/track.h"
#include "bsp/pid.h"
#include "bsp/remoteInfrared.h"
#include "sle_remote_server.h"
#include "app_init.h"
#include "watchdog.h"
#include "stdio.h"
#include "string.h"

#define SEND_CAR_INFO "{CAR_POWER:%.1f,CAR_SPEED:%d,CAR_CONN:%d}\n"

/* 外设初始化 */
void my_peripheral_init(void)
{
    user_beep_init();
    SSD1306_Init(); // OLED 显示屏初始化
    SSD1306_CLS();  // 清屏
    INA219_Init();
    PCF8574_Init();
    uapi_watchdog_disable();
    user_pwm_init();
    car_io_init();
    user_key_init();
    PID_Init();
    motor_enc_init();
    app_uart_init_config();
}

void car_state_report(void)
{
    uint8_t send_buf[100] = {0};
    msg_data_t msg_data = {0};
    memset(send_buf, 0, sizeof(send_buf));
    sprintf((char *)send_buf, SEND_CAR_INFO, (float)(systemValue.battery_voltage) / 1000.0, systemValue.car_speed,
            g_sle_conn_flag);
    msg_data.value = (uint8_t *)send_buf;
    msg_data.value_len = strlen((char *)send_buf);
    sle_server_send_report_by_handle(msg_data);
}
void *bsp_init(const char *arg)
{
    unused(arg);
    osal_msleep(500); /* 500: 延时500ms */
    sle_server_init();
    while (1) {
        INA219_get_bus_voltage_mv(&systemValue.battery_voltage);
        oled_show();
        car_state_report();
        osal_msleep(500); /* 500: 延时500ms */
    }
}

void *sle_control_task(const char *arg)
{
    unused(arg);

    user_ir_init();
    ir_timer_init();
    pid_timer_init();
    while (1) {
        printf("systemValue.R_enc:%d  %d\n", systemValue.L_enc, systemValue.R_enc);
        printf("return :%d %d\n", L_Motor_PWM, R_Motor_PWM);
        Remote_Infrared_KeyDeCode();
        osal_msleep(100);
    }
}

static void sle_server_entry(void)
{
    my_peripheral_init();
    osal_task *task_handle = NULL;
    osal_kthread_lock();

    task_handle = osal_kthread_create((osal_kthread_handler)bsp_init, 0, "bsp_init", SLE_SERVER_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, SLE_SERVER_TASK_PRIO);
        osal_kfree(task_handle);
    }

    task_handle =
        osal_kthread_create((osal_kthread_handler)sle_control_task, 0, "sle_control_task", SLE_SERVER_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, SLE_SERVER_TASK_PRIO);
        osal_kfree(task_handle);
    }

    osal_kthread_unlock();
}

/* Run the sle_entry. */
app_run(sle_server_entry);
