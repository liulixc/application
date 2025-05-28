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
#define CLIENT_ID "ws63_sm_lamp_0_0_2025031706"                                   // 设备id

#define MQTT_CMDTOPIC_SUB "$oc/devices/ws63_sm_lamp/sys/commands/#" // 平台下发命令

#define MQTT_DATATOPIC_PUB "$oc/devices/ws63_sm_lamp/sys/properties/report"                 // 属性上报topic
#define MQTT_CLIENT_RESPONSE "$oc/devices/ws63_sm_lamp/sys/commands/response/request_id=%s" // 命令响应topic

#define MQTT_DATA_SEND                                                                                              \
    "{\"services\": [{\"service_id\": \"control\",\"properties\": {\"light\" : \"%d\", \"lamp\" : \"%s\", \"red\" " \
    ":\"%d\", \"green\" : \"%d\", \"blue\" : \"%d\", \"light_auto_control\" : \"%s\"}}]}" // 上报
#define KEEP_ALIVE_INTERVAL 120
#define DELAY_TIME_MS 100

#define IOT

// 三色灯的PWM值
typedef struct _RGB_Value {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} RGB_Value_t;

// 三合一传感器
typedef struct _AP3216C_Value {
    uint16_t light;     // 光照强度
    uint16_t proximity; // 接近传感器
    uint16_t infrared;  // 人体红外传感器
} AP3216C_Value_t;

// 灯的状态
typedef enum _Lamp_Status {
    OFF_LAMP = 0,
    SUN_LIGHT_MODE, // 白光模式
    SLEEP_MODE,     // 睡眠模式
    READ_BOOK_MODE, // 看书模式
    LED_BLINK_MODE, // 闪烁模式
    SET_RGB_MODE,   //   RGB调光模式
    AUTO_MODE
} Lamp_Status_t;

typedef struct message_data {
    uint16_t lamp_delay_time; // 延时时间
    enum te_light_mode_t {
        LIGHT_MANUAL_MODE,   // 手动模式
        LIGHT_AUTO_MODE,     // 自动模式
    } is_auto_light_mode;    // 是否开启光照自动控制
    uint8_t led_light_value; // 灯的亮度控制值

    RGB_Value_t RGB_Value; // RGB灯控制

    Lamp_Status_t Lamp_Status;     // 控制灯是否开灯
    AP3216C_Value_t AP3216C_Value; // 三合一传感器的数据
} msg_data_t;

// 日期、时间
typedef struct date_time_value {
    uint16_t yaer;
    uint8_t month;
    uint8_t date;
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
} date_time_value_t;

extern msg_data_t sys_msg_data; // 传感器的数据
extern uint8_t mqtt_conn;
extern uint8_t g_cmdFlag;
extern char g_response_id[100]; // 保存命令id缓冲区
errcode_t mqtt_connect(void);
int mqtt_publish(const char *topic, char *msg);
#endif