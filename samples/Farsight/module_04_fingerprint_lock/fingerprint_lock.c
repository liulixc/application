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
#include "bsp/hal_bsp_aw2013.h"
#include "bsp/hal_bsp_ssd1306.h"
#include "bsp/hal_bsp_pcf8574.h"
#include "bsp/bsp_init.h"
#include "app_init.h"
#include "osal_task.h"

osThreadId_t Task1_ID; // 任务1设置为低优先级任务
osThreadId_t Task2_ID; // 任务1设置为低优先级任务
osThreadId_t Task3_ID; // 任务1设置为低优先级任务
osMutexId_t Mutex_ID;  // 定义互斥锁 ID
// 录入指纹信息
uint8_t cmd_input[12] = {0xEF, 0X01, 0XFF, 0XFF, 0xFF, 0xFF, 0X01, 0X00, 0X03, 0X01, 0X00, 0X05};
uint8_t cmd_generate1[13] = {0xEF, 0X01, 0XFF, 0XFF, 0xFF, 0xFF, 0X01, 0X00, 0X04, 0X02, 0X01, 0X00, 0X08};
uint8_t cmd_generate2[13] = {0xEF, 0X01, 0XFF, 0XFF, 0xFF, 0xFF, 0X01, 0X00, 0X04, 0X02, 0X02, 0X00, 0X09};
uint8_t cmd_merge[12] = {0xEF, 0X01, 0XFF, 0XFF, 0xFF, 0xFF, 0X01, 0X00, 0X03, 0X05, 0X00, 0X09};
uint8_t cmd_library[15] = {0xEF, 0X01, 0XFF, 0XFF, 0xFF, 0xFF, 0X01, 0X00, 0X06, 0X06, 0X01, 0X00, 0X01, 0X00, 0X0F};
uint8_t cmd_compare[] = {0xEF, 0X01, 0XFF, 0XFF, 0xFF, 0xFF, 0X01, 0X00, 0X03, 0X03, 0X00, 0X07}; // 指纹对比

void main_task(void *argument)
{
    unused(argument);
    while (1) {
        osMutexAcquire(Mutex_ID, 0xff); // 请求互斥锁
        oled_show();                    // 显示模块数据
#if (DEIVER_BOARD) // 如果定义了这个宏，就代表使用了驱动板 没有驱动板就不用定义
        PCF8574_Read(&systemdata.finger_int_io); // io拓展芯片引脚 状态读取
#endif
        osMutexRelease(Mutex_ID); // 释放互斥锁
        uapi_gpio_toggle(GPIO_13);

        osal_msleep(300);
    }
}
// 指纹锁的状态选择
void mode_select(void *argument)
{
    unused(argument);
    while (1) {
#if (DEIVER_BOARD)
        if (systemdata.finger_int_io & 0x10) { // 手指按下
            systemdata.key_status = STATUS_ON;
        } else {
            systemdata.key_status = STATUS_OFF;
        }
        if (systemdata.finger_int_io & 0x20) { // 指纹锁打开
            systemdata.lock_status = STATUS_ON;
        } else {
            systemdata.lock_status = STATUS_OFF;
        }
        osal_msleep(300);

#else
        if (uapi_gpio_get_val(FINGER_KEY) == 1) { // 手指按下
            systemdata.key_status = STATUS_ON;
        } else {
            systemdata.key_status = STATUS_OFF;
        }
        if (uapi_gpio_get_val(FINGER_LOCK) == 1) { // 指纹锁打开
            systemdata.lock_status = STATUS_ON;
        } else {
            systemdata.lock_status = STATUS_OFF;
        }
        osal_msleep(500);
#endif
    }
}
// 根据录入或测试模式进行操作1 先录入指纹，再对比指纹
void recv_task(void *argument)
{
    unused(argument);
    while (1) {

        if (systemdata.key_status) // 按下
        {
            if (systemdata.key_pattern == 0 && systemdata.input_flag == 0) // 录入
            {
                for (uint8_t j = 0; j < 2; j++) {
                    msg_data_t msg_data = {0};
                    uart_send_buff(cmd_input, sizeof(cmd_input));
                    osal_msleep(10);

                    recv_finger_hex(&msg_data);
                    if (msg_data.value != NULL && msg_data.value[9] == 0x00) {
                        printf("录入特征到缓冲区1\n");
                        if (j == 0) // 录入特征到缓冲区1
                        {
                            uart_send_buff(cmd_generate1, sizeof(cmd_generate1));
                            osal_msleep(10);
                            recv_finger_hex(&msg_data);
                        } else // 录入特征到缓冲区2
                        {
                            printf("录入特征到缓冲区2\n");
                            uart_send_buff(cmd_generate2, sizeof(cmd_generate2));
                            osal_msleep(10);
                            recv_finger_hex(&msg_data);

                            printf("开始合并指纹\n");
                            uart_send_buff(cmd_merge, sizeof(cmd_merge));
                            osal_msleep(10);
                            recv_finger_hex(&msg_data);
                            if (msg_data.value != NULL && msg_data.value[9] == 0x00) {
                                uart_send_buff(cmd_library, sizeof(cmd_library));
                                osal_msleep(10);
                                recv_finger_hex(&msg_data);
                                if (msg_data.value[9] == 0x00) {
                                    printf("采集成功！");
                                    osMutexAcquire(Mutex_ID, 0xff); // 请求互斥锁
                                    AW2013_Control_RGB(0, 0, 0xff);
                                    osMutexRelease(Mutex_ID); // 释放互斥锁
                                    systemdata.input_flag = 1;
                                    osal_msleep(100);
                                }
                            }
                        }
                    } else {
                        printf("采集失败！");
                        osMutexAcquire(Mutex_ID, 0xff); // 请求互斥锁
                        AW2013_Control_RGB(0xff, 0, 0);
                        osMutexRelease(Mutex_ID); // 释放互斥锁
                        uapi_gpio_toggle(BEEP);
                        osal_msleep(50);
                        uapi_gpio_toggle(BEEP);
                        osal_msleep(50);
                        uapi_gpio_toggle(BEEP);
                        osal_msleep(50);
                        uapi_gpio_toggle(BEEP);
                    }
                }
            } else {
                if (systemdata.key_pattern == 1 && systemdata.input_flag == 1) {
                    osal_msleep(500);
                    printf("开始测试！");
                    msg_data_t test_data = {0};
                    uart_send_buff(cmd_input, sizeof(cmd_input));
                    osal_msleep(10);
                    recv_finger_hex(&test_data);
                    if (test_data.value != NULL && test_data.value[9] == 0x00) {
                        uart_send_buff(cmd_generate2, sizeof(cmd_generate2));
                        osal_msleep(10);
                        recv_finger_hex(&test_data);
                        if (test_data.value != NULL && test_data.value[9] == 0x00) {
                            uart_send_buff(cmd_compare, sizeof(cmd_compare));
                            osal_msleep(10);
                            recv_finger_hex(&test_data);
                            if (test_data.value != NULL && test_data.value[9] == 0x00) {
                                printf("指纹正确，开锁！");
                                osMutexAcquire(Mutex_ID, 0xff); // 请求互斥锁
                                AW2013_Control_RGB(0, 0xff, 0);

#if (DEIVER_BOARD)
                                // 拓展io开锁
                                tmp_io.bit.p5 = 1;
                                PCF8574_Write(tmp_io.all);
                                osMutexRelease(Mutex_ID); // 释放互斥锁
#else
                                osMutexRelease(Mutex_ID); // 释放互斥锁
                                // 板载开锁
                                uapi_gpio_set_val(FINGER_LOCK, GPIO_LEVEL_HIGH);
#endif
                                osal_msleep(100);
                            } else {
                                printf("指纹错误！");
                                osMutexAcquire(Mutex_ID, 0xff); // 请求互斥锁
                                AW2013_Control_RGB(0xff, 0, 0);
                                osMutexRelease(Mutex_ID); // 释放互斥锁
                                uapi_gpio_toggle(BEEP);
                                osal_msleep(50);
                                uapi_gpio_toggle(BEEP);
                                osal_msleep(50);
                                uapi_gpio_toggle(BEEP);
                                osal_msleep(50);
                                uapi_gpio_toggle(BEEP);
#if (DEIVER_BOARD)
                                // 拓展io关锁
                                osMutexAcquire(Mutex_ID, 0xff); // 请求互斥锁
                                tmp_io.bit.p5 = 0;
                                PCF8574_Write(tmp_io.all);
                                osMutexRelease(Mutex_ID); // 释放互斥锁
#else
                                // 板载关锁
                                uapi_gpio_set_val(FINGER_LOCK, GPIO_LEVEL_LOW);
#endif
                                osal_msleep(100);
                            }
                        }
                    }
                } else {
                    if (systemdata.input_flag == 0) {
                        printf("指纹数据为空，请选择录入模式采集指纹！");
                    } else {
                        printf("请选择测试模式！");
                    }
                    osal_msleep(1000);
                }
            }
        }
        // 未按下
        else {
            printf("请根据当前模式进行操作\n");
            osal_msleep(1000);
        }
    }
}
/* 外设初始化 */
void my_peripheral_init(void)
{
    user_led_init();
    ssd1306_init(); // OLED 显示屏初始化
    ssd1306_cls();  // 清屏
    AW2013_Init();  // 三色LED灯的初始化]
#if (DEIVER_BOARD)
    PCF8574_Init();
#endif
    finger_io_init();
    user_key_init();
    app_uart_init_config();
}
static void fingerprint_lock_demo(void)
{
    // 创建互斥锁
    Mutex_ID = osMutexNew(NULL);
    if (Mutex_ID != NULL) {
        printf("ID = %d, Create Mutex_ID is OK!\n", Mutex_ID);
    }
    printf("Enter fingerprint_lock_demo()!\r\n");
    int ret = osal_msg_queue_create("uart_msg", msg_rev_size, &g_msg_queue, 0, msg_rev_size);
    if (ret != OSAL_SUCCESS) {
        printf("create queue failure!,error:%x\n", ret);
    }
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
    Task2_ID = osThreadNew((osThreadFunc_t)mode_select, NULL, &attr);
    if (Task1_ID != NULL) {
        printf("ID = %d, Create Task1_ID is OK!\r\n", Task2_ID);
    }
    attr.priority = osPriorityNormal2;
    Task3_ID = osThreadNew((osThreadFunc_t)recv_task, NULL, &attr);
    if (Task1_ID != NULL) {
        printf("ID = %d, Create Task1_ID is OK!\r\n", Task3_ID);
    }
}
app_run(fingerprint_lock_demo);
