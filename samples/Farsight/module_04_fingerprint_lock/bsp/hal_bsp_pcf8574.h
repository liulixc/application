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

#ifndef HAL_BSP_PCF8574_H
#define HAL_BSP_PCF8574_H

#include <stdbool.h>
#include "stdio.h"
#include "pinctrl.h"
#include "gpio.h"
#include "i2c.h"
#include "osal_task.h"
#include "securec.h"
#include "cmsis_os2.h"

#define PCF8574_I2C_ADDR 0x21 // 器件的I2C从机地址
#define PCF8574_I2C_ID 1      // 模块的I2C总线号
#define PCF8574_SPEED 100000  // 100KHz
#define I2C_MASTER_ADDR 0x0
/* io*/
#define I2C_SCL_MASTER_PIN 16
#define I2C_SDA_MASTER_PIN 15
#define CONFIG_PIN_MODE 2

// 使用位域定义IO扩展芯片的外部接口
typedef union _PCF8574_IO {
    unsigned char all;
    struct PCF8574_REG {
        unsigned char p0 : 1; // P0口
        unsigned char p1 : 1;
        unsigned char p2 : 1;
        unsigned char p3 : 1;

        unsigned char p4 : 1;
        unsigned char p5 : 1;
        unsigned char p6 : 1;
        unsigned char p7 : 1; // P7口
    } bit;
} tn_pcf8574_io_t;
extern tn_pcf8574_io_t tmp_io; // IO扩展芯片的引脚
/**
 * @brief  设置风扇的状态
 * @param  status: true or false
 * @retval None
 */
void set_fan(uint8_t status);

/**
 * @brief  设置蜂鸣器的状态
 * @param  status: true or false
 * @retval None
 */
void set_buzzer(uint8_t status);

/**
 * @brief  设置LED灯的状态
 * @param  status: true or false
 * @retval None
 */
void set_led(uint8_t status);
void get_flame(uint8_t status);

void set_D2(uint8_t status);
/**
 * @brief PCF8574 写数据
 *
 * 向PCF8574芯片写1个字节的数据，只能写1个字节的数据。
 *
 * @param data 数据中的每一位代表引脚 高位->低位，P7->P0
 * @return Returns {@link IOT_SUCCESS} 成功;
 *         Returns {@link IOT_FAILURE} 失败.
 */
uint32_t PCF8574_Write(uint8_t data);

/**
 * @brief PCF8574 读数据
 *
 * 向PCF8574芯片读1个字节的数据，只能读1个字节的数据。
 *
 * @param data 数据中的每一位代表引脚 高位->低位，P7->P0
 * @return Returns {@link IOT_SUCCESS} 成功;
 *         Returns {@link IOT_FAILURE} 失败.
 */
uint32_t PCF8574_Read(uint8_t *recv_data);
/**
 * @brief 初始化 iic
 *
 * @param
 * @return Returns {@link IOT_SUCCESS} 成功;
 *         Returns {@link IOT_FAILURE} 失败.
 */
uint32_t PCF8574_i2c_init(void);
/**
 * @brief 初始化 PCF8574
 *
 * @param
 * @return Returns {@link IOT_SUCCESS} 成功;
 *         Returns {@link IOT_FAILURE} 失败.
 */
uint32_t PCF8574_Init(void);
void pcf8574_toggle(void);
#endif
