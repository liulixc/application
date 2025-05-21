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

#ifndef MQTT_H
#define MQTT_H
#include "MQTTClientPersistence.h"
#include "MQTTClient.h"
#include "soc_osal.h"
#include "app_init.h"
#include "cmsis_os2.h"
#include "errcode.h"
#include "common_def.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SERVER_IP_ADDR "e2f41eb4ed.st1.iotda-device.cn-north-4.myhuaweicloud.com" // 接入地址
#define SERVER_IP_PORT 1883                                                       // 端口号
#define CLIENT_ID "smarthome_0_0_2025032003"                                      // 设备id

#define MQTT_CMDTOPIC_SUB "$oc/devices/smarthome/sys/commands/#" // 平台下发命令

#define MQTT_DATATOPIC_PUB "$oc/devices/smarthome/sys/properties/report"                 // 属性上报topic
#define MQTT_CLIENT_RESPONSE "$oc/devices/smarthome/sys/commands/response/request_id=%s" // 命令响应topic

#define MQTT_DATA_SEND                                                                                           \
    "{\"services\": [{\"service_id\": \"base\",\"properties\": {\"humidity\" : \"%.1f\",\"temperature\" : "   \
    "\"%.1f\",\"fan\" : \"%s\" ,   \"autoMode\" : \"%s\" ,     \"alsData\" : \"%d\" ,   \"is_Body\" : \"%s\" , " \
    "\"lamp_state\" : \"%s\" ,\"temp_threshold\" : \"%d\" , \"ps_threshold\" : \"%d\" ,   \"beep\" : \"%s\"}}]}" // 属性上报部分

#define KEEP_ALIVE_INTERVAL 120
#define DELAY_TIME_MS 100

#define IOT
typedef struct {
    uint16_t temp_threshold_value;
    uint16_t ps_threshold_value;
    uint16_t als_threshold_value;
} sensor_threshold;

typedef struct {
    int top;   // 上边距
    int left;  // 下边距
    int width; // 宽
    int hight; // 高
} margin_t;    // 边距类型

typedef struct message_data {
    uint8_t fanStatus; // 风扇的状态
    uint8_t beep_status;
    float humidity;    // 湿度值
    float temperature; // 温度值
    uint8_t is_Body;   // 有人或没人
    uint8_t lamp_state;
    uint16_t irData;
    uint16_t alsData;
    uint8_t touch_key;
    uint16_t psData; // 接近传感器值
    uint8_t smartControl_flag;
    sensor_threshold threshold_value;
} msg_data_t;

extern msg_data_t sys_msg_data; // 传感器的数据
extern uint8_t mqtt_conn;
extern uint8_t g_cmdFlag;
extern char g_response_id[100]; // 保存命令id缓冲区
errcode_t mqtt_connect(void);
int mqtt_publish(const char *topic, char *msg);
#endif