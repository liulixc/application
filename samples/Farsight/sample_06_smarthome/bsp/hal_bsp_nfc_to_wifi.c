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
#include <string.h>
#include <stdlib.h>
#include "cJSON.h"
#include "wifi_connect.h"
#include "hal_bsp_nfc.h"
#include "hal_bsp_nfc_to_wifi.h"
#define MAX_BUFF 64
#define TASK_INIT_DELAY 100

int NFC_configwifi(uint8_t *nfc_data, uint8_t len)
{
    char wifi_name[MAX_BUFF] = {0};   // WiFi名称
    char wifi_passwd[MAX_BUFF] = {0}; // WiFi密码
    char payload[128];

    memset_s(payload, 128, 0, 128);
    memcpy_s(payload, 128, nfc_data + 9, len - 9);
    printf("nfc payload = %s\r\n", payload);

    cJSON *root = cJSON_Parse(payload);
    cJSON *ssid = cJSON_GetObjectItem(root, "ssid");
    cJSON *password = cJSON_GetObjectItem(root, "password");
    if (root != NULL && ssid != NULL && password != NULL) {
        memcpy(wifi_name, ssid->valuestring, strlen(ssid->valuestring));
        memcpy(wifi_passwd, password->valuestring, strlen(password->valuestring));
        printf("wifi_passwd = %s, wifi_passwd = %s", wifi_passwd, wifi_passwd);
        if (wifi_connect(wifi_name, wifi_passwd) != ERRCODE_SUCC) {
            printf("nfc confifg fail! start retry!");
            if (wifi_connect(CONFIG_WIFI_SSID, CONFIG_WIFI_PWD) != ERRCODE_SUCC) {
                printf("wifi connect fail!");
                return -1;
            }
        }
        return 1;
    } else {
        printf("nfc no wifi data! start retry!");
        if (wifi_connect(CONFIG_WIFI_SSID, CONFIG_WIFI_PWD) != ERRCODE_SUCC) {
            printf("wifi connect fail!");
            return -1;
        } else {
            return 1;
        }
    }
}
int nfc_connect_wifi_init(void)
{
    // 通过NFC芯片进行连接WiFi
    uint8_t ndefLen = 0;     // ndef包的长度
    uint8_t ndef_Header = 0; // ndef消息开始标志位-用不到
    LCD_ShowString(80, 30, strlen((char *)"Connecting..."), 24, (uint8_t *)"Connecting...");
    // 读整个数据的包头部分，读出整个数据的长度
    if (NT3HReadHeaderNfc(&ndefLen, &ndef_Header) != true) {
        printf("NT3HReadHeaderNfc is failed.\r\n");
        return -1;
    }
    ndefLen += NDEF_HEADER_SIZE; // 加上头部字节
    if (ndefLen <= NDEF_HEADER_SIZE) {
        printf("ndefLen <= 2\r\n");
        return -1;
    }

    uint8_t *ndefBuff = (uint8_t *)malloc(ndefLen + 1);
    if (ndefBuff == NULL) {
        printf("ndefBuff malloc is Falied!\r\n");
        return -1;
    }
    if (get_NDEFDataPackage(ndefBuff, ndefLen) != ERRCODE_SUCC) {
        printf("get_NDEFDataPackage is failed.\r\n");
        return -1;
    }
    // 打印读出的数据
    printf("start print ndefBuff.\r\n");
    for (size_t i = 0; i < ndefLen; i++) {
        printf("0x%x ", ndefBuff[i]);
    }

    // 连接WiFi
    if (NFC_configwifi(ndefBuff, ndefLen) != 1) {
        LCD_DrawPicture(100, 60, 220, 180, (uint8_t *)gImage_conn_fail);
        LCD_ShowString(70, 180, strlen((char *)"WIFI CONN FAIL!"), 24, (uint8_t *)"WIFI CONN FAIL!");
        return -1;
    } else {
        printf("nfc connect wifi is SUCCESS\r\n");
        LCD_DrawPicture(100, 60, 220, 180, (uint8_t *)gImage_conn_succ);
        LCD_ShowString(70, 180, strlen((char *)"WIFI CONN SUCC!"), 24, (uint8_t *)"WIFI CONN SUCC!");
        return 0;
    }
}
