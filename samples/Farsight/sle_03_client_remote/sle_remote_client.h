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

#ifndef SLE_REMOTE_CLIENT_H
#define SLE_REMOTE_CLIENT_H
#include "osal_debug.h"
#include "osal_task.h"
#include "osal_addr.h"
#include "osal_wait.h"
#include "securec.h"
#include "gpio.h"
#include "pinctrl.h"
// 小车的当前状态值
typedef enum {
    CAR_STATUS_RUN = 0x01, // 前进
    CAR_STATUS_BACK,       // 后退
    CAR_STATUS_LEFT,       // 左转
    CAR_STATUS_RIGHT,      // 右转
    CAR_STATUS_STOP,       // 停止
    CAR_STATUS_L_SPEED,    // 低速行驶
    CAR_STATUS_M_SPEED,    // 中速行驶
    CAR_STATUS_H_SPEED,    // 高速行驶
} te_car_status_t;

typedef struct _system_value {
    te_car_status_t car_status; // 小车的状态
    uint16_t L_enc;             // 左电机的编码器值
    uint16_t R_enc;             // 右电机的编码器值
    uint16_t battery_voltage;   // 电池当前电压值
    uint16_t distance;          // 距离传感器
    uint8_t auto_abstacle_flag; // 是否开启避障功能
    uint8_t car_speed;          // 小车档位
} system_value_t;
/* 任务相关 */
#define SLE_CLIENT_TASK_PRIO 24
#define SLE_CLIENT_STACK_SIZE 0x1000
/* 串口接收数据结构体 */
typedef struct {
    uint8_t *value;
    uint16_t value_len;
} msg_data_t;

#endif