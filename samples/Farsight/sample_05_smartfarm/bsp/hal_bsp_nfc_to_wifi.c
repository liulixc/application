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
#include "hal_bsp_ssd1306.h"
#include "wifi_connect.h"
#include "hal_bsp_nfc.h"
#include "hal_bsp_nfc_to_wifi.h"
#include "oled_show_log.h"
#define MAX_BUFF 64
#define OFFSET_HEAD 5         // 头部字节偏移
#define OFFSET_WIFI_NAME 12   // 从数据类型的尾部到WiFi名称数据长度的偏移
#define OFFSET_WIFI_PASSWD 17 // 从WiFi名称数据的尾部到WiFi密码数据长度的偏移
#define TASK_INIT_DELAY 100
// 使用IOS系统下的NFC Tools软件配置网络
uint8_t ios_GetWiFi_ssid_passwd(const char *nfc_buff, char *wifi_name, char *wifi_passwd)
{
    uint8_t dataType_len = nfc_buff[NDEF_PROTOCOL_DATA_TYPE_LENGTH_OFFSET]; // 获取数据类型长度
    uint8_t ret = 0;
    uint8_t *dataType_buff = (uint8_t *)malloc(dataType_len + 1);
    if (dataType_buff == NULL) {
        return 0;
    }
    memset_s(dataType_buff, dataType_len + 1, 0, dataType_len + 1);
    // nfc_buff + OFFSET_HEAD，代表数组地址偏移5个字节
    if (memcpy_s(dataType_buff, dataType_len + 1, nfc_buff + OFFSET_HEAD, dataType_len) == 0) {
        printf("Use AppleIOS system..........\n");
        printf("dataType: %s\n", dataType_buff);
        // 使用IOS系统的 NFC Tools软件进行配网
        if (strcmp((char *)dataType_buff, "application/vnd.wfa.wsc") == 0) {
            // 判断是不是WiFi配网数据
            uint8_t wifi_name_len = nfc_buff[OFFSET_HEAD + dataType_len + OFFSET_WIFI_NAME];
            if (memcpy_s(wifi_name, MAX_BUFF, nfc_buff + OFFSET_HEAD + dataType_len + OFFSET_WIFI_NAME + 1,
                         wifi_name_len) == 0) {
                ret = 1; // 成功获取到WiFi名称和密码
            }
            uint8_t wifi_passwd_len =
                nfc_buff[OFFSET_HEAD + dataType_len + OFFSET_WIFI_NAME - 1 + wifi_name_len + OFFSET_WIFI_PASSWD];
            if (memcpy_s(wifi_passwd, MAX_BUFF,
                         nfc_buff + OFFSET_HEAD + dataType_len + OFFSET_WIFI_NAME + wifi_name_len + OFFSET_WIFI_PASSWD,
                         wifi_passwd_len) == 0) {
                ret = 1; // 成功获取到WiFi名称和密码
            }
        } else {
            ret = 0;
        }
    }
    free(dataType_buff);
    dataType_buff = NULL;

    return ret;
}

uint8_t wechat_GetWiFi_ssid_passwd(const char *nfc_buff, uint8_t *wifi_name, uint8_t *wifi_passwd)
{
    uint8_t payload_len = nfc_buff[NDEF_PROTOCOL_DATA_LENGTH_OFFSET]; // 获取数据长度
    uint8_t *payload = (uint8_t *)malloc(payload_len + 1);
    uint8_t ret = 0;

    if (payload == NULL) {
        printf("payload malloc failed.\r\n");
        return 0;
    }
    memset_s(payload, payload_len + 1, 0, payload_len + 1);
    memcpy_s(payload, payload_len + 1, nfc_buff + NDEF_PROTOCOL_VALID_DATA_OFFSET, payload_len);
    printf("Use Wechat system..........\n");
    printf("payload = %s\r\n", payload);

    cJSON *root = cJSON_Parse((char *)payload);
    cJSON *ssid = cJSON_GetObjectItem(root, "ssid");
    cJSON *password = cJSON_GetObjectItem(root, "passwd");
    if (root != NULL && ssid != NULL && password != NULL) {
        printf("ssid = %s, password = %s", ssid->valuestring, password->valuestring);
        if (strcpy_s((char *)wifi_name, strlen(ssid->valuestring) + 1, ssid->valuestring) == 0) {
            ret = 1;
        } else {
            ret = 0;
        }

        if (strcpy_s((char *)wifi_passwd, strlen(password->valuestring) + 1, password->valuestring) == 0) {
            ret = 1; // 成功获取到WiFi名称和密码
        } else {
            ret = 0;
        }
    }
    cJSON_Delete(root);
    free(payload);
    ssid = NULL;
    password = NULL;
    root = NULL;
    payload = NULL;

    return ret;
}

/**
 * @brief  使用NFC进行配网
 * @note   驱动NDEF协议中的第一个标签数据，然后进行配网
 *         配合《NFC调试文档.xlsx》，看下面的程序，更容易看懂
 * @param  *ndefBuff: 标签数据的缓冲区
 * @retval
 */
int NFC_configuresWiFiNetwork(char *ndefBuff)
{
    if (ndefBuff == NULL) {
        printf("NFC_configuresWiFiNetwork to ndefBuff is NULL\r\n");
        return -1;
    }
    uint8_t ret = 0;
    uint8_t wifi_name[MAX_BUFF] = {0};   // WiFi名称
    uint8_t wifi_passwd[MAX_BUFF] = {0}; // WiFi密码

    // 使用微信小程序进行配网
    if (ndefBuff[NDEF_PROTOCOL_DATA_TYPE_OFFSET] == 't') {
        ret = wechat_GetWiFi_ssid_passwd(ndefBuff, wifi_name, wifi_passwd);
    } else {
        ret = ios_GetWiFi_ssid_passwd(ndefBuff, (char *)wifi_name, (char *)wifi_passwd);
    }
    if (ret) {
        printf("wifi_name: %s\n", wifi_name);
        printf("wifi_passwd: %s\n", wifi_passwd);
        // 连接wifi
        if (wifi_connect((char *)wifi_name, (char *)wifi_passwd) == ERRCODE_SUCC) {

            ret = ERRCODE_SUCC;
        } else {

            ret = -1;
        }
    } else {

        return -1;
    }
    return ret;
}

int nfc_connect_wifi_init(void)
{
    // 通过NFC芯片进行连接WiFi
    uint8_t ndefLen = 0;     // ndef包的长度
    uint8_t ndef_Header = 0; // ndef消息开始标志位-用不到

    oled_consle_log("===start nfc===");

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
    oled_consle_log("=== end nfc ===");
    osDelay(TASK_INIT_DELAY);

    oled_consle_log("== start wifi==");
    // 连接WiFi
    if (NFC_configuresWiFiNetwork((char *)ndefBuff) != ERRCODE_SUCC) {
        printf("nfc connect wifi is failed!\r\n");
        oled_consle_log("== wifi fail ==");
        return -1;
    }
    printf("nfc connect wifi is SUCCESS\r\n");
    oled_consle_log("===wifi  succ===");
    osDelay(TASK_INIT_DELAY);
    SSD1306_CLS(); // 清屏
    return 0;
}