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

#include "stdio.h"
#include "string.h"
#include "soc_osal.h"
#include "i2c.h"
#include "securec.h"
#include "osal_debug.h"
#include "cmsis_os2.h"
#include "hal_bsp_nfc/hal_bsp_nfc.h"
#include "app_init.h"
osThreadId_t task1_ID; // 任务1设置为低优先级任务

#define TASK_DELAY_TIME 100
void task1(void)
{
    uint8_t ndefLen = 0;     // ndef包的长度
    uint8_t ndef_Header = 0; // ndef消息开始标志位-用不到
    size_t i = 0;
    // 对 NFC（近场通信）模块进行初始化
    nfc_Init();
    osDelay(TASK_DELAY_TIME);
    printf("I2C Test Start\r\n");

    // 读取整个数据的包头部分，获取整个数据的长度

    if (NT3HReadHeaderNfc(&ndefLen, &ndef_Header) != true) {
        printf("NT3HReadHeaderNfc is failed.\r\n");
        return;
    }
    // 将 ndefLen 加上 NDEF_HEADER_SIZE，因为之前获取的长度不包含头部字节
    ndefLen += NDEF_HEADER_SIZE;
    // 检查 ndefLen 是否小于等于 NDEF_HEADER_SIZE 如果是，说明数据长度异常，可能没有有效数据
    if (ndefLen <= NDEF_HEADER_SIZE) {
        printf("ndefLen <= 2\r\n");
        return;
    }
    // 使用 malloc 函数动态分配一块内存，用于存储读取的 NDEF 数据
    uint8_t *ndefBuff = (uint8_t *)malloc(ndefLen + 1);
    if (ndefBuff == NULL) {
        printf("ndefBuff malloc is Falied!\r\n");
        return;
    }
    // 将 NDEF 数据读取到 ndefBuff 中
    if (get_NDEFDataPackage(ndefBuff, ndefLen) != ERRCODE_SUCC) {
        printf("get_NDEFDataPackage is failed. \r\n");
        return;
    }

    printf("start print ndefBuff.\r\n");
    // 使用 for 循环遍历 ndefBuff 数组，打印每个字节的数据
    for (i = 0; i < ndefLen; i++) {
        printf("0x%x ", ndefBuff[i]);
    }

    while (1) {
        osDelay(TASK_DELAY_TIME);
    }
}
static void base_nfc_demo(void)
{
    printf("Enter base_nfc_demo()!\r\n");

    osThreadAttr_t attr;
    attr.name = "Task1";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 0x2000;
    attr.priority = osPriorityNormal;

    task1_ID = osThreadNew((osThreadFunc_t)task1, NULL, &attr);
    if (task1_ID != NULL) {
        printf("ID = %d, Create task1_ID is OK!\r\n", task1_ID);
    }
}
app_run(base_nfc_demo);