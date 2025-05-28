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

#ifndef HAL_BSP_SC12B_H
#define HAL_BSP_SC12B_H

#include "cmsis_os2.h"
#include "stdio.h"
#include "pinctrl.h"
#include "gpio.h"
#include "i2c.h"
#include "securec.h"
#define SC12B_I2C_ADDR 0x40    // 器件的I2C从机地址
#define SC12B_I2C_IDX 1        // 模块的I2C总线号
#define SC12B_I2C_SPEED 100000 // 100KHz
#define I2C_MASTER_ADDR 0x0
/* io */
#define I2C_SCL_MASTER_PIN 16
#define I2C_SDA_MASTER_PIN 15
#define CONFIG_PIN_MODE 2

#define SENSET0 0x0    // 通道4灵敏度寄存器
#define SENSETCOM 0x01 // 其他灵敏度寄存器
#define CTRL0 0x02     // 控制寄存器
#define CTRL1 0x03     // 控制寄存器
#define OUTPUT1 0x08   // 按键状态输出寄存器
#define OUTPUT2 0x09   // 按键状态输出寄存器
#define SAMPH 0x0A     // 采样值寄存器
#define SAMPL 0x0B     // 采样值寄存器
uint32_t sc12b_init(void);
uint32_t SC12B_WiteCmdByteData(uint8_t regAddr, uint8_t byte);
uint32_t SC12B_ReadRegByteData(uint8_t regAddr, uint16_t *byte);
void get_key_info(uint16_t key_value);
#endif
