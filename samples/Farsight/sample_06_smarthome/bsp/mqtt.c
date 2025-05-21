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
#include "bsp_init.h"
#include "mqtt.h"
#include "cJSON.h"
#include "hal_bsp_bmps.h"
#ifdef IOT
char *g_username = "smarthome";
char *g_password = "59f9b9ffffbf18d629b77781ce025f00663295b60ea929955258c0ea009fdbb2";
#endif

uint8_t g_cmdFlag;
msg_data_t sys_msg_data = {0}; // 传感器的数据
uint8_t mqtt_conn;
char g_send_buffer[512] = {0}; // 发布数据缓冲区
char g_response_id[100] = {0}; // 保存命令id缓冲区

MQTTClient client;
volatile MQTTClient_deliveryToken deliveredToken;
extern int MQTTClient_init(void);

/* 回调函数，处理连接丢失 */
void connlost(void *context, char *cause)
{
    unused(context);
    printf("Connection lost: %s\n", cause);
}
int mqtt_subscribe(const char *topic)
{
    printf("subscribe start\r\n");
    MQTTClient_subscribe(client, topic, 1);
    return 0;
}

int mqtt_publish(const char *topic, char *msg)
{
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;
    int ret = 0;
    pubmsg.payload = msg;
    pubmsg.payloadlen = (int)strlen(msg);
    pubmsg.qos = 1;
    pubmsg.retained = 0;
    // printf("[payload]:  %s, [topic]: %s\r\n", msg, topic);
    ret = MQTTClient_publishMessage(client, topic, &pubmsg, &token);
    if (ret != MQTTCLIENT_SUCCESS) {
        printf("mqtt publish failed\r\n");
        return ret;
    }

    return ret;
}

/* 回调函数，处理消息到达 */
void delivered(void *context, MQTTClient_deliveryToken dt)
{
    unused(context);
    printf("Message with token value %d delivery confirmed\n", dt);

    deliveredToken = dt;
}
// 解析字符串并保存到数组中
void parse_after_equal(const char *input, char *output)
{
    const char *equalsign = strchr(input, '=');
    if (equalsign != NULL) {
        // 计算等于号后面的字符串长度
        strcpy(output, equalsign + 1);
    }
}
/* 回调函数，处理接收到的消息 */
int messageArrived(void *context, char *topic_name, int topic_len, MQTTClient_message *message)
{
    unused(context);
    unused(topic_len);
    unused(topic_name);
    cJSON *root = cJSON_Parse((char *)message->payload);
    cJSON *paras = cJSON_GetObjectItem(root, "paras");
    printf("[Message]: %s\n", (char *)message->payload);
    // 进行传感器控制
    if (strstr((char *)message->payload, "autoMode") != NULL) {
        if (strstr((char *)message->payload, "ON") != NULL) {
            sys_msg_data.smartControl_flag = 1;
        } else if (strstr((char *)message->payload, "OFF") != NULL) {
            sys_msg_data.smartControl_flag = 0;
        }
    } else if (strstr((char *)message->payload, "fan") != NULL) {
        if (strstr((char *)message->payload, "ON") != NULL) {
            sys_msg_data.fanStatus = 1;
            sys_msg_data.touch_key = 1;
        } else if (strstr((char *)message->payload, "OFF") != NULL) {
            sys_msg_data.fanStatus = 0;
            sys_msg_data.touch_key = 0;
        }
    } else if (strstr((char *)message->payload, "lamp_mode") != NULL) {
        if (strstr((char *)message->payload, "ON") != NULL) {
            sys_msg_data.lamp_state = 1;

        } else if (strstr((char *)message->payload, "OFF") != NULL) {
            sys_msg_data.lamp_state = 0;
        }
    } else if (strstr((char *)message->payload, "beep_mode") != NULL) {
        if (strstr((char *)message->payload, "ON") != NULL) {
            sys_msg_data.beep_status = 1;

        } else if (strstr((char *)message->payload, "OFF") != NULL) {
            sys_msg_data.beep_status = 0;
        }
    } else if (strstr((char *)message->payload, "threshold_value") != NULL) {
        // 解析阈值上下限
        cJSON *ps = cJSON_GetObjectItem(paras, "ps");
        if (ps) {
            sys_msg_data.threshold_value.ps_threshold_value = ps->valueint;
        }
        cJSON *temp = cJSON_GetObjectItem(paras, "temp");
        if (temp) {
            sys_msg_data.threshold_value.temp_threshold_value = temp->valueint;
        }
        cJSON *als = cJSON_GetObjectItem(paras, "als");
        if (als) {
            sys_msg_data.threshold_value.als_threshold_value = als->valueint;
        }
        ps = NULL;
        temp = NULL;
        als = NULL;
    }

    // 解析命令id
    parse_after_equal(topic_name, g_response_id);
    g_cmdFlag = 1;
    memset((char *)message->payload, 0, message->payloadlen);

    return 1; // 表示消息已被处理
}

errcode_t mqtt_connect(void)
{
    int ret;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    /* 初始化MQTT客户端 */

    MQTTClient_init();
    /* 创建 MQTT 客户端 */
    ret = MQTTClient_create(&client, SERVER_IP_ADDR, CLIENT_ID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    if (ret != MQTTCLIENT_SUCCESS) {
        printf("Failed to create MQTT client, return code %d\n", ret);
        return ERRCODE_FAIL;
    }
    conn_opts.keepAliveInterval = KEEP_ALIVE_INTERVAL;
    conn_opts.cleansession = 1;
#ifdef IOT
    conn_opts.username = g_username;
    conn_opts.password = g_password;
#endif
    // 绑定回调函数
    MQTTClient_setCallbacks(client, NULL, connlost, messageArrived, delivered);

    // 尝试连接
    if ((ret = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        LCD_DrawPicture(100, 60, 220, 180, (uint8_t *)gImage_conn_fail);
        LCD_ShowString(70, 180, strlen((char *)"MQTT CONN FAIL!"), 24, (uint8_t *)"MQTT CONN FAIL!");
        MQTTClient_destroy(&client); // 连接失败时销毁客户端
        ili9341_Clear(WHITE);
        mqtt_conn = 2;
        return ERRCODE_FAIL;
    }
    LCD_DrawPicture(100, 60, 220, 180, (uint8_t *)gImage_conn_succ);
    LCD_ShowString(70, 180, strlen((char *)"MQTT CONN SUCC!"), 24, (uint8_t *)"MQTT CONN SUCC!");
    osDelay(DELAY_TIME_MS);
    // 订阅MQTT主题
    ili9341_Clear(WHITE);
    mqtt_conn = 1;
    osDelay(100); // 需要延时 否则会发布失败
    while (1) {
        // "{\"services\": [{\"service_id\": \"control\",\"properties\": {\"humidity\" : \"%.1f\",\"temperature\" : "
        // "\"%.1f\",\"fan\" : \"%s\" ,   \"autoMode\" : \"%s\" ,     \"alsData\" : \"%d\" ,   \"is_Body\" : \"%s\" , "
        // "\"lamp_state\" : \"%s\" ,\"temp_threshold\" : \"%d\" , \"ps_threshold\" : \"%d\" ,   \"beep\" : \"%s\" }}]}"
        // // 属性上报部分
        osDelay(DELAY_TIME_MS);
        memset(g_send_buffer, 0, sizeof(g_send_buffer) / sizeof(g_send_buffer[0]));
        sprintf(g_send_buffer, MQTT_DATA_SEND, sys_msg_data.humidity, sys_msg_data.temperature,
                sys_msg_data.fanStatus ? "ON" : "OFF", sys_msg_data.smartControl_flag ? "ON" : "OFF",
                sys_msg_data.alsData, sys_msg_data.is_Body ? "YES" : "NO", sys_msg_data.lamp_state ? "ON" : "OFF",
                sys_msg_data.threshold_value.temp_threshold_value, sys_msg_data.threshold_value.ps_threshold_value,
                sys_msg_data.beep_status ? "ON" : "OFF");
        mqtt_publish(MQTT_DATATOPIC_PUB, g_send_buffer);
        memset(g_send_buffer, 0, sizeof(g_send_buffer) / sizeof(g_send_buffer[0]));
    }
    return ERRCODE_SUCC;
}