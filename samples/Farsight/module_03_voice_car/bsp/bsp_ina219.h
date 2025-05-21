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
#ifndef __BSP_INA219_H__
#define __BSP_INA219_H__

#include <stdbool.h>
#include "stdio.h"
#include "pinctrl.h"
#include "gpio.h"
#include "i2c.h"
#include "osal_task.h"
#include "securec.h"
#include "cmsis_os2.h"

#define INA219_I2C_ADDR 0x41    // 器件的I2C从机地址
#define INA219_I2C_IDX 1        // 模块的I2C总线号
#define INA219_I2C_SPEED 100000 // 100KHz

// 获取电压值
uint16_t INA219_get_bus_voltage_mv(uint16_t *busvolt);

// 获取电流值
uint16_t INA219_get_current_ma(void);

// 获取电源功率值
uint16_t INA219_get_power_mw(void);
// 初始化函数
uint32_t INA219_Init(void);

#endif
