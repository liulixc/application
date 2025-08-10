/**
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2023-2023. All rights reserved.
 *
 * Description: Timer Sample Source. \n
 * 文件功能：定时器示例程序
 * 本文件演示了如何使用WS63芯片的定时器功能，包括：
 * 1. 定时器初始化和配置
 * 2. 创建多个定时器实例
 * 3. 设置定时器超时回调函数
 * 4. 启动和停止定时器
 * 5. 测量定时器精度
 *
 * 模块功能概述：
 * - 创建4个不同延时时间的定时器（1ms、2ms、3ms、4ms）
 * - 通过回调函数记录定时器触发时间
 * - 计算并输出实际延时与设定延时的差异
 * - 演示定时器的基本使用流程
 *
 * History: \n
 * 2023-07-18, Create file. \n
 */
/* 系统头文件包含 */
#include "timer.h"        /* 定时器驱动接口 */
#include "tcxo.h"         /* 晶振时钟接口，用于获取系统时间 */
#include "chip_core_irq.h" /* 芯片中断相关定义 */
#include "common_def.h"   /* 通用定义和宏 */
#include "soc_osal.h"     /* 操作系统抽象层接口 */
#include "app_init.h"     /* 应用程序初始化接口 */

/* 定时器相关配置宏定义 */
#define TIMER_TIMERS_NUM            4       /* 创建的定时器数量 */
#define TIMER_INDEX                 1       /* 使用的定时器索引号 */
#define TIMER_PRIO                  1       /* 定时器中断优先级 */
#define TIMER_DELAY_INT             5       /* 定时器启动间隔时间(ms) */
#define TIMER1_DELAY_1000US         1000    /* 定时器1延时时间：1000微秒(1ms) */
#define TIMER2_DELAY_2000US         2000    /* 定时器2延时时间：2000微秒(2ms) */
#define TIMER3_DELAY_3000US         3000    /* 定时器3延时时间：3000微秒(3ms) */
#define TIMER4_DELAY_4000US         4000    /* 定时器4延时时间：4000微秒(4ms) */
#define TIMER_MS_2_US               1000    /* 毫秒到微秒的转换系数 */

/* 任务相关配置宏定义 */
#define TIMER_TASK_PRIO             24      /* 定时器任务优先级 */
#define TIMER_TASK_STACK_SIZE       0x1000  /* 定时器任务栈大小(4KB) */

/**
 * @brief 定时器信息结构体
 * 用于记录每个定时器的时间信息，包括开始时间、结束时间和设定的延时时间
 */
typedef struct timer_info {
    uint32_t start_time;    /* 定时器启动时的系统时间戳(ms) */
    uint32_t end_time;      /* 定时器超时时的系统时间戳(ms) */
    uint32_t delay_time;    /* 设定的延时时间(us) */
} timer_info_t;

/* 全局变量定义 */
static uint32_t g_timer_int_count = 0;  /* 定时器中断触发计数器 */

/* 定时器信息数组，存储4个定时器的配置和运行信息 */
static timer_info_t g_timers_info[TIMER_TIMERS_NUM] = {
    {0, 0, TIMER1_DELAY_1000US},  /* 定时器0：延时1000us */
    {0, 0, TIMER2_DELAY_2000US},  /* 定时器1：延时2000us */
    {0, 0, TIMER3_DELAY_3000US},  /* 定时器2：延时3000us */
    {0, 0, TIMER4_DELAY_4000US}   /* 定时器3：延时4000us */
};

/**
 * @brief 定时器超时回调函数
 * 当定时器超时时，系统会调用此函数
 * 
 * @param data 传入的用户数据，这里是定时器索引号
 * 
 * 功能说明：
 * 1. 获取当前系统时间作为定时器结束时间
 * 2. 增加中断计数器
 * 3. 用于后续计算定时器的实际延时时间
 */
static void timer_timeout_callback(uintptr_t data)
{
    uint32_t timer_index = (uint32_t)data;  /* 获取定时器索引 */
    g_timers_info[timer_index].end_time = uapi_tcxo_get_ms();  /* 记录超时时间 */
    g_timer_int_count++;  /* 增加中断计数 */
}

/**
 * @brief 定时器测试任务主函数
 * 执行定时器的完整测试流程
 * 功能流程：
 * 1. 初始化定时器模块
 * 2. 配置定时器适配器（中断号和优先级）
 * 3. 创建并启动4个不同延时的定时器
 * 4. 等待所有定时器超时
 * 5. 停止并删除定时器
 * 6. 输出测试结果（实际延时vs设定延时）
 */
static void *timer_task(const char *arg)
{
    unused(arg);  /* 标记参数未使用，避免编译警告 */
    
    /* 定时器句柄数组，用于管理4个定时器实例 */
    timer_handle_t timer_index[TIMER_TIMERS_NUM] = { 0 };
    
    /* 步骤1：初始化定时器模块 */
    uapi_timer_init();
    
    /* 步骤2：配置定时器适配器，设置中断号和优先级 */
    uapi_timer_adapter(TIMER_INDEX, TIMER_1_IRQN, TIMER_PRIO);

    /* 步骤3：创建并启动所有定时器 */
    for (uint32_t i = 0; i < TIMER_TIMERS_NUM; i++) {
        /* 创建定时器实例 */
        uapi_timer_create(TIMER_INDEX, &timer_index[i]);
        
        /* 记录定时器启动时间 */
        g_timers_info[i].start_time = uapi_tcxo_get_ms();
        
        /* 启动定时器，设置延时时间和回调函数 */
        uapi_timer_start(timer_index[i], g_timers_info[i].delay_time, timer_timeout_callback, i);
        
        /* 延时一段时间再启动下一个定时器，避免同时启动 */
        osal_msleep(TIMER_DELAY_INT);
    }

    /* 步骤4：等待所有定时器超时 */
    while (g_timer_int_count < TIMER_TIMERS_NUM) {
        osal_msleep(TIMER_DELAY_INT);  /* 轮询等待，避免CPU空转 */
    }

    /* 步骤5：清理资源并输出测试结果 */
    for (uint32_t i = 0; i < TIMER_TIMERS_NUM; i++) {
        /* 停止定时器 */
        uapi_timer_stop(timer_index[i]);
        
        /* 删除定时器实例，释放资源 */
        uapi_timer_delete(timer_index[i]);
        
        /* 输出测试结果：实际延时时间 */
        osal_printk("real time[%d] = %dms  ", i, (g_timers_info[i].end_time -  g_timers_info[i].start_time));
        
        /* 输出设定的延时时间（转换为毫秒） */
        osal_printk("  delay = %dms\r\n", g_timers_info[i].delay_time / TIMER_MS_2_US);
    }
    
    return NULL;
}


static void timer_entry(void)
{
    osal_task *task_handle = NULL;
    
    /* 获取内核线程锁，保证任务创建的原子性 */
    osal_kthread_lock();
    
    /* 创建定时器测试任务 */
    task_handle = osal_kthread_create((osal_kthread_handler)timer_task, 0, "TimerTask", TIMER_TASK_STACK_SIZE);
    
    if (task_handle != NULL) {
        /* 设置任务优先级 */
        osal_kthread_set_priority(task_handle, TIMER_TASK_PRIO);
        
        /* 释放任务句柄内存（任务已经启动，句柄不再需要） */
        osal_kfree(task_handle);
    }
    
    /* 释放内核线程锁 */
    osal_kthread_unlock();
}

/* 注册并运行定时器示例程序入口函数 */
/* 该宏会在系统启动时自动调用timer_entry函数 */
app_run(timer_entry);