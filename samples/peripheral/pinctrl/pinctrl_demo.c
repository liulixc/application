/**
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2023-2023. All rights reserved.
 *
 * Description: PINCTRL Sample Source. \n
 *
 * History: \n
 * 2023-07-27, Create file. \n
 */
#include "pinctrl.h"
#include "common_def.h"
#include "soc_osal.h"
#include "app_init.h"

#define PINCTRL_PIN_MODE            5   // 要设置的引脚模式
#define PINCTRL_PIN_DS              3   // 要设置的驱动能力
#define PINCTRL_PIN_PULL            2   // 要设置的上下拉状态

#define PINCTRL_TASK_PRIO           24  // 任务优先级
#define PINCTRL_TASK_STACK_SIZE     0x1000 // 任务栈大小

/**
 * @brief PINCTRL演示任务
 * 依次演示引脚模式、驱动能力、上下拉的获取与设置流程。
 * @param arg 任务参数，未使用
 * @return NULL
 */
static void *pinctrl_task(const char *arg)
{
    unused(arg); // 防止未使用参数告警
    pin_t pin = CONFIG_PINCTRL_USE_PIN; // 获取配置的演示引脚编号
    pin_mode_t mode;
    pin_drive_strength_t ds;
    pin_pull_t pull;

    /* PINCTRL init. 初始化PINCTRL模块 */
    uapi_pin_init();

    // 1. 获取并打印当前引脚模式
    osal_printk("start get pin<%d> mode!\r\n", pin);
    mode = uapi_pin_get_mode(pin);
    osal_printk("the mode of pin<%d> is %d.\r\n", pin, mode);
    // 2. 设置引脚模式并校验
    mode = PINCTRL_PIN_MODE;
    osal_printk("start set pin<%d> mode<%d>!\r\n", pin, mode);
    if (uapi_pin_set_mode(pin, mode) == ERRCODE_SUCC && uapi_pin_get_mode(pin) == mode) {
        osal_printk("set pin<%d> mode<%d> succ.\r\n", pin, mode);
    }

    osal_printk("\r\n");
    // 3. 获取并打印当前驱动能力
    osal_printk("start get pin<%d> driver-strength!\r\n", pin);
    ds = uapi_pin_get_ds(pin);
    osal_printk("The driver-strength of pin<%d> is %d.\r\n", pin, ds);
    // 4. 设置驱动能力并校验
    ds = PINCTRL_PIN_DS;
    osal_printk("start set pin<%d> driver-strength<%d>!\r\n", pin, ds);
    if (uapi_pin_set_ds(pin, ds) == ERRCODE_SUCC && uapi_pin_get_ds(pin) == ds) {
        osal_printk("set pin<%d> driver-strength<%d> succ.\r\n", pin, ds);
    }

    osal_printk("\r\n");
    // 5. 获取并打印当前上下拉状态
    osal_printk("start get pin<%d> pull/down status!\r\n", pin);
    pull = uapi_pin_get_pull(pin);
    osal_printk("The pull/down status of pin<%d> is %d.\r\n", pin, pull);
    // 6. 设置上下拉状态并校验
    pull = PINCTRL_PIN_PULL;
    osal_printk("start set pin<%d> pull/down status<%d>!\r\n", pin, pull);
    if (uapi_pin_set_pull(pin, pull) == ERRCODE_SUCC && uapi_pin_get_pull(pin) == pull) {
        osal_printk("set pin<%d> pull/down status<%d> succ.\r\n", pin, pull);
    }

    /* PINCTRL deinit. 反初始化，释放资源 */
    uapi_pin_deinit();

    return NULL;
}

/**
 * @brief PINCTRL演示入口，创建并启动pinctrl_task任务
 */
static void pinctrl_entry(void)
{
    osal_task *task_handle = NULL;
    osal_kthread_lock(); // 进入临界区，防止多线程冲突
    // 创建任务，任务函数为pinctrl_task
    task_handle = osal_kthread_create((osal_kthread_handler)pinctrl_task, 0, "PinctrlTask", PINCTRL_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, PINCTRL_TASK_PRIO); // 设置任务优先级
        osal_kfree(task_handle); // 释放任务句柄内存
    }
    osal_kthread_unlock(); // 退出临界区
}

/*
 * @brief 应用入口，运行pinctrl_entry，启动PINCTRL演示
 */
app_run(pinctrl_entry);