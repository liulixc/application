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

#include "hal_bsp_sgp30.h"
#include "stdio.h"
#include "pinctrl.h"
#include "gpio.h"
#include "i2c.h"
#include "osal_task.h"
#include "securec.h"
#define SHT20_HoldMaster_Temp_REG_ADDR 0xE3 // 主机模式会阻塞其他IIC设备的通信
#define SHT20_HoldMaster_Humi_REG_ADDR 0xE5
#define SHT20_NoHoldMaster_Temp_REG_ADDR 0x2008
#define SHT20_NoHoldMaster_Humi_REG_ADDR 0xF5
#define SHT20_W_USER_REG_ADDR 0xE6
#define SHT20_R_USER_REG_ADDR 0xE7
#define SHT20_SW_REG_ADDR 0x06

#define TEMP_LEFT_SHIFT_8 8

#define HUMI_LEFT_SHIFT_8 8
#define READ_TEMP_DATA_NUM 6
// 读从机设备的数据
static uint32_t SGP30_RecvData(uint8_t *data, size_t size)
{
    i2c_data_t i2cData = {0};
    i2cData.receive_buf = data;
    i2cData.receive_len = size;

    return uapi_i2c_master_read(SGP30_I2C_IDX, SGP30_I2C_ADDR, &i2cData);
}
// 向从机设备 发送数据
static uint32_t SGP30_WiteByteData(uint8_t byte1, uint8_t byte2)
{
    uint8_t buffer[] = {byte1, byte2};
    i2c_data_t i2cData = {0};
    i2cData.send_buf = buffer;
    i2cData.send_len = sizeof(buffer);

    return uapi_i2c_master_write(SGP30_I2C_IDX, SGP30_I2C_ADDR, &i2cData);
}

// 获取CO2值
void SGP30_ReadData(uint16_t *temp, uint16_t *humi)
{
    uint32_t result;
    uint8_t buffer[6] = {0};

    /* 发送检测命令 */
    SGP30_WiteByteData(0x20, 0x08);
    osal_msleep(100);

    // 读数据
    result = SGP30_RecvData(buffer, READ_TEMP_DATA_NUM);
    if (result != 0) {
        printf("sgp status = 0x%x!!!\r\n", result);
    }

    // 看芯片手册，手册中有转换公式的说明
    *temp = (buffer[0] << TEMP_LEFT_SHIFT_8) | buffer[1];

    *humi = (buffer[3] << HUMI_LEFT_SHIFT_8) | buffer[4];
    memset_s(buffer, sizeof(buffer), 0, sizeof(buffer));
}

void SGP30_Init(void)
{
    // uint32_t result;
    // uint32_t baudrate = SGP30_I2C_SPEED;
    // uint32_t hscode = I2C_MASTER_ADDR;
    // uapi_pin_set_mode(I2C_SCL_MASTER_PIN, CONFIG_PIN_MODE);
    // uapi_pin_set_mode(I2C_SDA_MASTER_PIN, CONFIG_PIN_MODE);
    // uapi_pin_set_pull(I2C_SCL_MASTER_PIN, PIN_PULL_TYPE_UP);
    // uapi_pin_set_pull(I2C_SDA_MASTER_PIN, PIN_PULL_TYPE_UP);

    // result = uapi_i2c_master_init(SGP30_I2C_IDX, baudrate, hscode);
    // if (result != ERRCODE_SUCC) {
    //     printf("I2C Init status is 0x%x!!!\r\n", result);
    // }

    SGP30_WiteByteData(0x20, 0x03);
}
