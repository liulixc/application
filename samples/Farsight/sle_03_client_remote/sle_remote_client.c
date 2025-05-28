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
#include "bsp/hal_bsp_sc12b.h"
#include "bsp/bsp_st7789_4line.h"
#include "bsp/sle_client.h"
#include "sle_remote_client.h"
#include "app_init.h"
#include "watchdog.h"
#include "stdio.h"
#include "string.h"
#include "bsp/remoteInfrared.h"

system_value_t systemValue = {0}; // 系统全局变量
unsigned long g_msg_queue = 0;
unsigned int g_msg_rev_size = sizeof(msg_data_t);
ssapc_write_param_t *sle_uart_send_param = NULL;
uint8_t select_mode;
uint8_t last_value;
void user_key_init(void);
// 配置功能函数，根据接收到的命令字符串执行相应的配置操作
void config_func(char *buf)
{
    if ((strstr(buf, "CAR_ADD") != NULL) | (strstr(buf, "CAR_SUBTRACT") != NULL) | (strstr(buf, "CAR_RETURN") != NULL) |
        (strstr(buf, "CAR_MENU") != NULL)) {
        // 若包含上述任意一个字符串，延时 300 毫秒
        osal_mdelay(300);
    } else {
        osal_mdelay(20);
    }
}
// 板级支持包初始化任务函数，负责初始化硬件和处理按键输入
void *bsp_init(const char *arg)
{
    unused(arg);
    uint16_t key_value = 0;
    char send_buf[100] = {0};
    msg_data_t msg_data = {0};
    user_key_init();
    sc12b_init();    // 初始化 SC12B 设备
    user_ir_start(); // 启动用户红外功能
    while (1) {
        SC12B_ReadRegByteData(OUTPUT1, &key_value); // 从 SC12B 设备的 OUTPUT1 寄存器读取一个字节的数据到 key_value 中
        memset(send_buf, 0, sizeof(send_buf)); // 清空发送缓冲区
        memcpy(send_buf, get_key_info(key_value), sizeof(send_buf));
        if (select_mode) {               // 如果处于红外模式
            osal_kthread_lock();         // 上锁，否则红外发送会出问题
            if (last_value != key_value) // 如果当前按键值与上一次不同
                ir_send_str(send_buf);
            osal_kthread_unlock();
        } else { // 如果处于星闪模式
            msg_data.value = (uint8_t *)send_buf;
            msg_data.value_len = strlen(send_buf);
            osal_msg_queue_write_copy(g_msg_queue, (void *)&msg_data, g_msg_rev_size,
                                      0); // 将消息数据结构体复制到消息队列中
        }
        config_func(send_buf);  // 调用配置功能函数，根据发送缓冲区的内容进行配置操作
        last_value = key_value; // 更新上一次的按键值
    }
    return NULL;
}
// SLE 控制任务函数，负责处理从消息队列中读取的消息并发送到 SLE 设备
void *sle_control_task(const char *arg)
{
    unused(arg);
    sle_client_init();
    while (1) {
        msg_data_t msg_data = {0};
        // 从消息队列中读取消息并复制到 msg_data 中，等待时间为永久
        int msg_ret = osal_msg_queue_read_copy(g_msg_queue, &msg_data, &g_msg_rev_size, OSAL_WAIT_FOREVER);
        if (msg_ret != OSAL_SUCCESS) {
            printf("msg queue read copy fail.");
            if (msg_data.value != NULL) {
                osal_vfree(msg_data.value);
            }
        }
        // 如果消息数据结构体的 value 成员不为空
        if (msg_data.value != NULL) {
            ssapc_write_param_t *sle_uart_send_param = &g_sle_send_param;
            sle_uart_send_param->data_len = msg_data.value_len;
            sle_uart_send_param->data = msg_data.value;
            ssapc_write_cmd(0, g_sle_uart_conn_id, sle_uart_send_param); // 发送命令到从机
        }
    }
    return NULL;
}

void *sle_display_task(const char *arg)
{
    unused(arg);
    app_spi_init_pin();
    app_spi_master_init_config();
    ST7789_Init();
    while (1) {
        if (sle_recv_flag) {
            car_state_Parse(sle_recv);
            sle_recv_flag = 0;
        }
        remote_display();
        osal_mdelay(300);
    }
}
static void sle_client_entry(void)
{
    osal_task *task_handle = NULL;
    uapi_watchdog_disable(); // 关闭看门狗
    osal_kthread_lock();
    int ret = osal_msg_queue_create("sle_msg", g_msg_rev_size, &g_msg_queue, 0, g_msg_rev_size);
    if (ret != OSAL_SUCCESS) {
        printf("create queue failure!,error:%x\n", ret);
    }
    task_handle = osal_kthread_create((osal_kthread_handler)bsp_init, 0, "bsp_init", SLE_CLIENT_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, SLE_CLIENT_TASK_PRIO);
        osal_kfree(task_handle);
    }
    task_handle =
        osal_kthread_create((osal_kthread_handler)sle_control_task, 0, "sle_control_task", SLE_CLIENT_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, SLE_CLIENT_TASK_PRIO);
        osal_kfree(task_handle);
    }
    task_handle =
        osal_kthread_create((osal_kthread_handler)sle_display_task, 0, "sle_display_task", SLE_CLIENT_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, SLE_CLIENT_TASK_PRIO);
        osal_kfree(task_handle);
    }
    osal_kthread_unlock();
}

/* Run the sle_uart_entry. */
app_run(sle_client_entry);

void user_key_callback_func(pin_t pin, uintptr_t param)
{
    unused(pin);
    unused(param);
    select_mode = !select_mode;
}
void user_key_init(void)
{
    uapi_pin_set_mode(GPIO_03, PIN_MODE_0);

    uapi_pin_set_mode(GPIO_03, 0);
    uapi_pin_set_pull(GPIO_03, PIN_PULL_TYPE_DOWN);
    uapi_gpio_set_dir(GPIO_03, GPIO_DIRECTION_INPUT);
    /* 注册指定GPIO下降沿中断，回调函数为gpio_callback_func */
    if (uapi_gpio_register_isr_func(GPIO_03, GPIO_INTERRUPT_RISING_EDGE, user_key_callback_func) != 0) {
        printf("register_isr_func fail!\r\n");
        uapi_gpio_unregister_isr_func(GPIO_03);
    }
    uapi_gpio_enable_interrupt(GPIO_03);
}