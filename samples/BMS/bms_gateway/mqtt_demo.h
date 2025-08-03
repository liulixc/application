#ifndef MQTT_DEMO_H
#define MQTT_DEMO_H

// ======================== 头文件包含 ========================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// MQTT和操作系统相关头文件
#include "MQTTClient.h"
#include "MQTTClientPersistence.h"
#include "osal_debug.h"
#include "los_memory.h"
#include "los_task.h"
#include "soc_osal.h"
#include "app_init.h"
#include "common_def.h"
#include "wifi_connect.h"
#include "watchdog.h"
#include "cJSON.h"
#include "l610.h"
#include "sle_client.h"
#include "monitor.h"
#include "ota_huawei.h"

// 网络类型枚举定义
typedef enum {
    NET_TYPE_4G,
    NET_TYPE_WIFI,
    NET_TYPE_NONE
} net_type_t;

// 最大支持的BMS设备数
#define MAX_BMS_DEVICES 32 // 根据实际情况，网关最多可管理的设备数

typedef struct {
    char receive_payload[256];
} MQTT_msg;

typedef struct {
    int temperature[5]; // 温度
    int current;     // 电流
    int cell_voltages[12]; // 电池电压
    int total_voltage; // 总电压
    uint8_t soc; // SOC
    uint8_t level; // 节点层级
    char child[32]; // 子节点MAC地址后两位
} environment_msg;

// ======================== 函数声明 ========================

// MQTT回调函数
void delivered(void *context, MQTTClient_deliveryToken dt);
int msgArrved(void *context, char *topic_name, int topic_len, MQTTClient_message *message);
void connlost(void *context, char *cause);

// MQTT操作函数
int mqtt_publish_multi_device(const char *topic);
int mqtt_publish(const char *topic, const char *payload);
int mqtt_subscribe(const char *topic);
int mqtt_connect(void);

// 网络切换函数
int switch_to_wifi(const char *ssid, const char *psk);
void switch_to_4g(void);

// 主任务函数
int mqtt_task(void);

#endif