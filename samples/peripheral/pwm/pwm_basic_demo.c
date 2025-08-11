/**
 * @file pwm_basic_demo.c
 * @brief PWM基础演示程序
 * 
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2023-2023. All rights reserved.
 *
 * 本文件实现了PWM模块的基础功能演示，包括：
 * - PWM初始化和配置
 * - PWM通道开启和关闭
 * - PWM中断处理
 * - PWM组功能使用（适用于V151版本）
 * 
 * 该演示程序展示了PWM的基本使用方法，适合初学者了解PWM模块的工作原理。
 * 
 * History:
 * 2023-06-27, Create file.
 */
// ==================== 头文件包含 ====================
#if defined(CONFIG_PWM_SUPPORT_LPM)
#include "pm_veto.h"          // 低功耗管理相关头文件
#endif

#include "common_def.h"        // 通用定义和宏
#include "pinctrl.h"           // 引脚控制接口
#include "pwm.h"               // PWM驱动接口
#include "tcxo.h"              // 晶振控制接口
#include "soc_osal.h"          // 操作系统抽象层
#include "app_init.h"          // 应用初始化接口

// ==================== 宏定义 ====================
#define TEST_TCXO_DELAY_1000MS     1000    // 测试延时1000毫秒
#define PWM_TASK_PRIO              24      // PWM任务优先级
#define PWM_TASK_STACK_SIZE        0x1000  // PWM任务栈大小（4KB）

static errcode_t pwm_sample_callback(uint8_t channel)
{
    osal_printk("PWM %d, cycle done. \r\n", channel);
    return ERRCODE_SUCC;
}

static void *pwm_task(const char *arg)
{
    UNUSED(arg);
    // PWM配置结构体
    // low_time: 100 (低电平时间)
    // high_time: 100 (高电平时间) 
    // offset_time: 0 (偏移时间)
    // cycles: 0xFF (周期数：255个周期)
    // repeat: false (不重复执行)
    pwm_config_t cfg_no_repeat = {
        .low_time = 100,    // 低电平时间：100个时钟周期
        .high_time = 100,   // 高电平时间：100个时钟周期
        .offset_time = 0,   // 偏移时间：0
        .cycles = 0xFF,     // 周期数：255个周期
        .repeat = false     // 不重复执行
    };

    uapi_pin_set_mode(CONFIG_PWM_PIN, CONFIG_PWM_PIN_MODE);
    uapi_pwm_deinit();
    uapi_pwm_init();
    uapi_pwm_open(CONFIG_PWM_CHANNEL, &cfg_no_repeat);

    uapi_tcxo_delay_ms((uint32_t)TEST_TCXO_DELAY_1000MS);
    uapi_pwm_unregister_interrupt(CONFIG_PWM_CHANNEL);
    uapi_pwm_register_interrupt(CONFIG_PWM_CHANNEL, pwm_sample_callback);
#ifdef CONFIG_PWM_USING_V151
    uint8_t channel_id = CONFIG_PWM_CHANNEL;
    /* channel_id can also choose to configure multiple channels, and the third parameter also needs to be adjusted
        accordingly. */
    uapi_pwm_set_group(CONFIG_PWM_GROUP_ID, &channel_id, 1);
    /* Here you can also call the uapi_pwm_start interface to open each channel individually. */
    uapi_pwm_start_group(CONFIG_PWM_GROUP_ID);
#else
    uapi_pwm_start(CONFIG_PWM_CHANNEL);
#endif

    uapi_tcxo_delay_ms((uint32_t)TEST_TCXO_DELAY_1000MS);
#ifdef CONFIG_PWM_USING_V151
    uapi_pwm_close(CONFIG_PWM_GROUP_ID);
#else
    uapi_pwm_close(CONFIG_PWM_CHANNEL);
#endif

    uapi_tcxo_delay_ms((uint32_t)TEST_TCXO_DELAY_1000MS);
    uapi_pwm_deinit();
    return NULL;
}

static void pwm_entry(void)
{
    osal_task *task_handle = NULL;
    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)pwm_task, 0, "PwmTask", PWM_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, PWM_TASK_PRIO);
    }
    osal_kthread_unlock();
}

/* Run the pwm_entry. */
app_run(pwm_entry);