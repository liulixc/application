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
#include "osal_debug.h"
#include "cmsis_os2.h"
#include "hal_bsp_sc12b/hal_bsp_sc12b.h"
#include "app_init.h"

osThreadId_t task1_ID; // 任务1

#define DELAY_TIME_MS 10
#define INT_IO GPIO_14
uint16_t current_state, prev_state;
uint8_t int_flag;
void press_callback_func(pin_t pin, uintptr_t param)
{
    unused(pin);
    unused(param);
    int_flag = 1;
}
void user_key_init(void)
{
    /* 配置引脚为下拉，并设置为输入模式 */
    uapi_pin_set_mode(INT_IO, PIN_MODE_0);
    uapi_pin_set_pull(INT_IO, PIN_PULL_TYPE_DOWN);
    uapi_gpio_set_dir(INT_IO, GPIO_DIRECTION_INPUT);
    /* 注册指定GPIO下降沿中断，回调函数为gpio_callback_func */
    if (uapi_gpio_register_isr_func(INT_IO, GPIO_INTERRUPT_RISING_EDGE, press_callback_func) != 0) {
        printf("register_isr_func fail!\r\n");
        uapi_gpio_unregister_isr_func(INT_IO);
    }
    /* 使能中断 */
    uapi_gpio_enable_interrupt(INT_IO);
}
void task1(void)
{
    // 初始化触摸芯片
    sc12b_init();
    // 初始化按键中断引脚
    user_key_init();
    while (1) {
        // 如果发生中断，则读取按键键值
        if (int_flag) {
            SC12B_ReadRegByteData(OUTPUT1, &current_state);
            // 获取按键的功能
            get_key_info(current_state);
            // 记录上次按下的按键
            prev_state = current_state;
            // 清除标志位
            int_flag = 0;
        }

        osDelay(DELAY_TIME_MS);
    }
}
static void base_sc12b_demo(void)
{
    printf("Enter base_sc12b_demo()!\r\n");

    osThreadAttr_t attr;
    attr.name = "task1";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 0x2000;
    attr.priority = osPriorityNormal;

    task1_ID = osThreadNew((osThreadFunc_t)task1, NULL, &attr);
    if (task1_ID != NULL) {
        printf("ID = %d, Create task1_ID is OK!\r\n", task1_ID);
    }
}
app_run(base_sc12b_demo);