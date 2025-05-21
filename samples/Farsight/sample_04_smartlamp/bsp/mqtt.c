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
#include "hal_bsp_ssd1306.h"
#include "oled_show_log.h"
#include "cJSON.h"
#ifdef IOT
char *g_username = "ws63_sm_lamp";
char *g_password = "d522979d0b5371b2f7c24330783c02461d9ac7c5c6d1f6f929039262a9431d8d";
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
    printf("[payload]:  %s, [topic]: %s\r\n", msg, topic);
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
    if (strstr((char *)message->payload, "lamp_mode") != NULL) {
        sys_msg_data.is_auto_light_mode = LIGHT_MANUAL_MODE;
        if (strstr((char *)message->payload, "SUN_LIGHT_MODE_ON") != NULL) {
            sys_msg_data.Lamp_Status = SUN_LIGHT_MODE;
        } else if (strstr((char *)message->payload, "SLEEP_MODE_ON") != NULL) {
            sys_msg_data.Lamp_Status = SLEEP_MODE;
        } else if (strstr((char *)message->payload, "READ_BOOK_MODE_ON") != NULL) {
            sys_msg_data.Lamp_Status = READ_BOOK_MODE;
        } else if (strstr((char *)message->payload, "LED_BLINK_MODE_ON") != NULL) {
            sys_msg_data.Lamp_Status = LED_BLINK_MODE;
        } else {
            sys_msg_data.Lamp_Status = OFF_LAMP;
        }
    } else if (strstr((char *)message->payload, "auto_mode") != NULL) {
        if (strstr((char *)message->payload, "ON") != NULL) {
            sys_msg_data.is_auto_light_mode = LIGHT_AUTO_MODE;
        } else if (strstr((char *)message->payload, "OFF") != NULL) {
            sys_msg_data.is_auto_light_mode = LIGHT_MANUAL_MODE;
        }

    } else if (strstr((char *)message->payload, "RGB") != NULL) {
        if (sys_msg_data.is_auto_light_mode == LIGHT_MANUAL_MODE) {

            cJSON *red = cJSON_GetObjectItem(paras, "red");
            cJSON *green = cJSON_GetObjectItem(paras, "green");
            cJSON *blue = cJSON_GetObjectItem(paras, "blue");
            sys_msg_data.RGB_Value.red = red->valueint;
            sys_msg_data.RGB_Value.green = green->valueint;
            sys_msg_data.RGB_Value.blue = blue->valueint;
            sys_msg_data.Lamp_Status = SET_RGB_MODE;
            red = green = blue = NULL;
        }
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
    oled_consle_log("===mqtt init===");
    MQTTClient_init();
    /* 创建 MQTT 客户端 */
    ret = MQTTClient_create(&client, SERVER_IP_ADDR, CLIENT_ID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    if (ret != MQTTCLIENT_SUCCESS) {
        printf("Failed to create MQTT client, return code %d\n", ret);
        oled_consle_log("=mqtt cre fail=");
        return ERRCODE_FAIL;
    }
    oled_consle_log("= mqtt cre ok =");
    conn_opts.keepAliveInterval = KEEP_ALIVE_INTERVAL;
    conn_opts.cleansession = 1;
#ifdef IOT
    conn_opts.username = g_username;
    conn_opts.password = g_password;
#endif
    // 绑定回调函数
    MQTTClient_setCallbacks(client, NULL, connlost, messageArrived, delivered);
    oled_consle_log("=mqtt set  cbk=");

    // 尝试连接
    if ((ret = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        oled_consle_log("mqtt conn fail");
        printf("Failed to connect, return code %d\n", ret);
        MQTTClient_destroy(&client); // 连接失败时销毁客户端
        return ERRCODE_FAIL;
    }
    oled_consle_log("=mqtt conn  ok=");
    printf("Connected to MQTT broker!\n");
    osDelay(DELAY_TIME_MS);
    // 订阅MQTT主题
    SSD1306_CLS(); // 清屏
    mqtt_conn = 1;
    osDelay(100); // 需要延时 否则会发布失败
    while (1) {
        //  "{\"services\": [{\"service_id\": \"control\",\"properties\": {\"light\" : \"%d\", \"lamp\" : \"%s\",
        //  \"red\":\"%d\", \"green\" : \"%s\", \"blue\" : \"%d\", \"light_auto_control\" : \"%s\"}}]}" // 上报
        // 属性上报部分
        osDelay(DELAY_TIME_MS);
        memset(g_send_buffer, 0, sizeof(g_send_buffer) / sizeof(g_send_buffer[0]));
        sprintf(g_send_buffer, MQTT_DATA_SEND, 1024 - sys_msg_data.AP3216C_Value.light,
                sys_msg_data.Lamp_Status ? "ON" : "OFF", sys_msg_data.RGB_Value.red, sys_msg_data.RGB_Value.green,
                sys_msg_data.RGB_Value.blue, sys_msg_data.is_auto_light_mode ? "ON" : "OFF");
        mqtt_publish(MQTT_DATATOPIC_PUB, g_send_buffer);
        memset(g_send_buffer, 0, sizeof(g_send_buffer) / sizeof(g_send_buffer[0]));
        osDelay(DELAY_TIME_MS); // 需要延时 否则会发布失败
    }
    return ERRCODE_SUCC;
}