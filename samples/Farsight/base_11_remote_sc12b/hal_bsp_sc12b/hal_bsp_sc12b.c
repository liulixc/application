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

#include "hal_bsp_sc12b.h"

// 向寄存器中写数据
uint32_t SC12B_WiteCmdByteData(uint8_t regAddr, uint8_t byte)
{
    uint8_t buffer[] = {regAddr, byte};
    i2c_data_t i2cData = {0};
    i2cData.send_buf = buffer;
    i2cData.send_len = sizeof(buffer);
    return uapi_i2c_master_write(SC12B_I2C_IDX, SC12B_I2C_ADDR, &i2cData);
}

// 读从机设备数据
static uint32_t SC12B_RecvData(uint8_t *data, size_t size)
{
    i2c_data_t i2cData = {0};
    i2cData.receive_buf = data;
    i2cData.receive_len = size;

    return uapi_i2c_master_read(SC12B_I2C_IDX, SC12B_I2C_ADDR, &i2cData);
}

// 向从机设备 发送数据
static uint32_t SC12B_WiteByteData(uint8_t byte)
{
    uint8_t buffer[] = {byte};
    i2c_data_t i2cData = {0};
    i2cData.send_buf = &byte;
    i2cData.send_len = sizeof(buffer);
    return uapi_i2c_master_write(SC12B_I2C_IDX, SC12B_I2C_ADDR, &i2cData);
}
// 读寄存器中的数据   参数: regAddr 目标寄存器地址, byte: 取到的数据
uint32_t SC12B_ReadRegByteData(uint8_t regAddr, uint16_t *byte)
{
    uint32_t result = 0;
    uint8_t buffer[2] = {0};

    // 写命令
    result = SC12B_WiteByteData(regAddr);
    if (result != ERRCODE_SUCC) {
        printf("I2C SC12B status = 0x%x!!!\r\n", result);
        return result;
    }

    // 读数据
    result = SC12B_RecvData(buffer, 2);
    if (result != ERRCODE_SUCC) {
        printf("I2C SC12B status = 0x%x!!!\r\n", result);
        return result;
    }
    *byte = buffer[1] | buffer[0] << 8; // 键值 1111111111110000b

    return ERRCODE_SUCC;
}
// 初始化
uint32_t sc12b_init(void)
{
    uint32_t result;
    uint32_t baudrate = SC12B_I2C_SPEED;
    uint32_t hscode = I2C_MASTER_ADDR;
    uapi_pin_set_mode(I2C_SCL_MASTER_PIN, CONFIG_PIN_MODE);
    uapi_pin_set_mode(I2C_SDA_MASTER_PIN, CONFIG_PIN_MODE);
    uapi_pin_set_pull(I2C_SCL_MASTER_PIN, PIN_PULL_TYPE_UP);
    uapi_pin_set_pull(I2C_SDA_MASTER_PIN, PIN_PULL_TYPE_UP);

    result = uapi_i2c_master_init(SC12B_I2C_IDX, baudrate, hscode);
    if (result != ERRCODE_SUCC) {
        printf("I2C Init status is 0x%x!!!\r\n", result);
        return result;
    }
    return ERRCODE_SUCC;
}
void get_key_info(uint16_t key_value)
{
    uint16_t current = key_value;
    switch (current) {
        case 0x2000:
            printf("关机\n");
            break;
        case 0x200:
            printf("主菜单\n");
            break;
        case 0x1000:
            printf("前\n");
            break;
        case 0x400:
            printf("后\n");
            break;
        case 0x4000:
            printf("左\n");
            break;
        case 0x100:
            printf("右\n");
            break;
        case 0x800:
            printf("OK\n");
            break;
        case 0x8000:
            printf("减\n");
            break;
        case 0x80:
            printf("加\n");
            break;
        case 0x10:
            printf("设置\n");
            break;
        case 0x20:
            printf("返回\n");
            break;
        case 0x40:
            printf("功能\n");
            break;
        default:
            break;
    }
}
