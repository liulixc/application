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

#include <stdio.h>
#include <unistd.h>
#include "hal_bsp_pcf8574.h"

tn_pcf8574_io_t tmp_io = {0}; // IO扩展芯片的引脚

uint32_t PCF8574_Write(const uint8_t send_data)
{
    uint32_t result = 0;
    uint8_t buffer[] = {send_data};

    i2c_data_t i2c_data = {0};
    i2c_data.send_buf = buffer;
    i2c_data.send_len = sizeof(buffer);

    result = uapi_i2c_master_write(PCF8574_I2C_ID, PCF8574_I2C_ADDR, &i2c_data);
    if (result != ERRCODE_SUCC) {
        printf("I2C PCF8574 Write result is 0x%x!!!\r\n", result);
        printf(" i2c_data.send_buf 0x%x!!!\r\n", i2c_data.send_buf);
        return result;
    }
    return result;
}
uint32_t PCF8574_Read(uint8_t *recv_data)
{
    uint32_t result = 0;
    i2c_data_t i2c_data = {0};
    i2c_data.receive_buf = recv_data;
    i2c_data.receive_len = 1;

    result = uapi_i2c_master_read(PCF8574_I2C_ID, PCF8574_I2C_ADDR, &i2c_data);
    if (result != ERRCODE_SUCC) {
        printf("I2C PCF8574 Read result is 0x%x!!!\r\n", result);
        printf("i2c_data.receive_buf == %x!!!\r\n", i2c_data.receive_buf);
        return result;
    }
    return result;
}
uint32_t PCF8574_i2c_init(void)
{
    uint32_t result;
    uint32_t baudrate = PCF8574_SPEED;
    uint32_t hscode = I2C_MASTER_ADDR;
    uapi_pin_set_mode(I2C_SCL_MASTER_PIN, CONFIG_PIN_MODE);
    uapi_pin_set_mode(I2C_SDA_MASTER_PIN, CONFIG_PIN_MODE);
    uapi_pin_set_pull(I2C_SCL_MASTER_PIN, PIN_PULL_TYPE_UP);
    uapi_pin_set_pull(I2C_SDA_MASTER_PIN, PIN_PULL_TYPE_UP);

    result = uapi_i2c_master_init(PCF8574_I2C_ID, baudrate, hscode);
    if (result != ERRCODE_SUCC) {
        printf("I2C Init status is 0x%x!!!\r\n", result);
        return result;
    }
    return result;
}
uint32_t PCF8574_Init(void)
{
    uint32_t result;

    tmp_io.bit.p0 = 0; // 可燃气ad
    tmp_io.bit.p1 = 0;
    tmp_io.bit.p2 = 0;
    tmp_io.bit.p3 = 0;
    tmp_io.bit.p4 = 1; // 火焰传感器检测
    tmp_io.bit.p5 = 1; // 可燃气传感器使能
    tmp_io.bit.p6 = 0;
    tmp_io.bit.p7 = 1; // 可燃气传感器检测

    result = PCF8574_Write(tmp_io.all);
    if (result != ERRCODE_SUCC) {
        printf("pcf8574 init fail!!!\r\n", result);
        return result;
    }
    return result;
}