/**
 * @file mqtt_demo.c
 * @brief MQTT客户端示例程序
 * @details 实现了MQTT客户端的基本功能，包括：
 *          - 连接到华为云IoT平台
 *          - 订阅和发布消息
 *          - 环境数据采集和上报
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MQTTClient.h"
#include "MQTTClientPersistence.h"
#include "osal_debug.h"
#include "MQTTClient.h"
#include "los_memory.h"
#include "los_task.h"
#include "soc_osal.h"
#include "app_init.h"
#include "common_def.h"
#include "wifi_connect.h"
#include "watchdog.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include "cjson_demo.h"
#include "aht20_test.h"
#include "aht20.h"
#include "pwm_demo.h"
#include "mqtt_demo.h"

// MQTT服务器连接地址（华为云IoT平台）
#define ADDRESS "tcp://2502007d6f.st1.iotda-device.cn-north-4.myhuaweicloud.com"
// 客户端ID，用于唯一标识设备
#define CLIENTID "680b91649314d11851158e8d_Battery01_0_0_2025042603"
// MQTT主题名称
#define TOPIC "MQTT Examples"
// 测试消息内容
#define PAYLOAD "Hello World!"
// 服务质量等级：1表示至少发送一次
#define QOS 1

// WiFi配置信息
#define CONFIG_WIFI_SSID "QQ"       // WiFi名称
#define CONFIG_WIFI_PWD "tangyuan"  // WiFi密码

// 任务相关配置
#define MQTT_STA_TASK_PRIO 24           // MQTT任务优先级
#define MQTT_STA_TASK_STACK_SIZE 0x1000 // MQTT任务栈大小
#define TIMEOUT 10000L                   // 超时时间：10秒
#define MSG_MAX_LEN 28                  // 消息最大长度
#define MSG_QUEUE_SIZE 32               // 消息队列大小

/**
 * @brief 全局变量定义
 */
static unsigned long g_msg_queue;        // 消息队列句柄
volatile MQTTClient_deliveryToken deliveredtoken;  // 消息投递令牌
// 设备认证信息
char *g_username = "680b91649314d11851158e8d_Battery01"; // 设备ID
char *g_password = "50f670e657058bb33c23b92a633720a7fbbfba36f493f263c346b55bb2fb8bf3"; // 设备密码
MQTTClient client;                      // MQTT客户端实例
extern int MQTTClient_init(void);       // MQTT客户端初始化函数声明

/**
 * @brief 消息投递回调函数
 * @param context 上下文信息（未使用）
 * @param dt 投递令牌
 */
void delivered(void *context, MQTTClient_deliveryToken dt)
{
    unused(context);
    printf("Message with token value %d delivery confirmed\r\n", dt);
    deliveredtoken = dt;
}

/**
 * @brief 消息到达回调函数
 * @param context 上下文信息（未使用）
 * @param topic_name 消息主题
 * @param topic_len 主题长度
 * @param message MQTT消息结构体
 * @return 1表示成功处理，-1表示处理失败
 * @note 该函数负责处理接收到的MQTT消息，并将消息写入消息队列
 */
int msgArrved(void *context, char *topic_name, int topic_len, MQTTClient_message *message)
{
    unused(context);
    unused(topic_len);
    MQTT_msg *receive_msg = osal_kmalloc(sizeof(MQTT_msg), 0);
    printf("mqtt_message_arrive() success, the topic is %s, the payload is %s \n", topic_name, message->payload);
    receive_msg->msg_type = EN_MSG_PARS;
    receive_msg->receive_payload = message->payload;
    uint32_t ret = osal_msg_queue_write_copy(g_msg_queue, receive_msg, sizeof(MQTT_msg), OSAL_WAIT_FOREVER);
    if (ret != 0) {
        printf("ret = %#x\r\n", ret);
        osal_kfree(receive_msg);
        return -1;
    }
    return 1;
}

/**
 * @brief MQTT连接断开回调函数
 * @param context 上下文信息（未使用）
 * @param cause 断开连接的原因
 */
void connlost(void *context, char *cause)
{
    unused(context);
    printf("mqtt_connection_lost() error, cause: %s\n", cause);
}

/**
 * @brief 订阅MQTT主题
 * @param topic 要订阅的主题
 * @return 0表示成功，其他值表示失败
 */
int mqtt_subscribe(const char *topic)
{
    printf("subscribe start\r\n");
    int rc = MQTTClient_subscribe(client, topic, QOS);
    if (rc != MQTTCLIENT_SUCCESS) {
        printf("Failed to subscribe, return code %d\n", rc);
        return -1;
    }
    printf("Subscribe success\r\n");
    return rc;
}

/**
 * @brief 发布MQTT消息
 * @param topic 发布的主题
 * @param report_msg 要发送的消息内容，包含温度和湿度数据
 * @return MQTTCLIENT_SUCCESS表示成功，其他值表示失败
 * @note 函数会动态分配内存用于JSON消息，使用完后会释放
 */
int mqtt_publish(const char *topic, MQTT_msg *report_msg)
{
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;
    int rc = 0;
    char *msg = make_json("environment", report_msg->temp, report_msg->humi);
    pubmsg.payload = msg;
    pubmsg.payloadlen = (int)strlen(msg);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;
    rc = MQTTClient_publishMessage(client, topic, &pubmsg, &token);
    if (rc != MQTTCLIENT_SUCCESS) {
        printf("mqtt_publish failed\r\n");
    }
    printf("mqtt_publish(), the payload is %s, the topic is %s\r\n", msg, topic);
    osal_kfree(msg);
    msg = NULL;
    return rc;
}

/**
 * @brief 连接MQTT服务器
 * @return MQTTCLIENT_SUCCESS表示成功，-1表示失败
 * @note 该函数会初始化MQTT客户端，设置连接参数并建立连接
 *       同时会注册消息回调函数用于处理收到的消息
 */
int mqtt_connect(void)
{
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;
    printf("start mqtt sync subscribe...\r\n");
    MQTTClient_init();
    MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    conn_opts.keepAliveInterval = 120; // 保持间隔为120秒，每120秒发送一次消息
    conn_opts.cleansession = 1;
    if (g_username != NULL) {
        conn_opts.username = g_username;
        conn_opts.password = g_password;
    }
    MQTTClient_setCallbacks(client, NULL, connlost, msgArrved, delivered);
    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        printf("Failed to connect, return code %d\n", rc);
        return -1;
    }
    printf("connect success\r\n");
    return rc;
}

/**
 * @brief MQTT任务主函数
 * @return 执行结果，0表示成功，其他值表示失败
 * @note 该任务负责：
 *       1. 连接WiFi网络
 *       2. 建立MQTT连接
 *       3. 订阅命令主题
 *       4. 处理消息队列中的数据
 */
int mqtt_task(void)
{
    MQTT_msg *report_msg = osal_kmalloc(sizeof(MQTT_msg), 0);
    int ret = 0;
    uint32_t resize = 32;
    char *beep_status = NULL;
    wifi_connect(CONFIG_WIFI_SSID, CONFIG_WIFI_PWD);
    ret = mqtt_connect();
    if (ret != 0) {
        printf("connect failed, result %d\n", ret);
    }
    osal_msleep(1000); // 1000等待连接成功
    char *cmd_topic = combine_strings(3, "$oc/devices/", g_username, "/sys/commands/#");
    ret = mqtt_subscribe(cmd_topic);
    if (ret < 0) {
        printf("subscribe topic error, result %d\n", ret);
    }
    char *report_topic = combine_strings(3, "$oc/devices/", g_username, "/sys/properties/report");
    while (1) {
        ret = osal_msg_queue_read_copy(g_msg_queue, report_msg, &resize, OSAL_WAIT_FOREVER);
        if (ret != 0) {
            printf("queue_read ret = %#x\r\n", ret);
            osal_kfree(report_msg);
            break;
        }
        if (report_msg != NULL) {
            printf("report_msg->msg_type = %d, report_msg->temp = %s\r\n", report_msg->msg_type, report_msg->temp);
            switch (report_msg->msg_type) {
                case EN_MSG_PARS:
                    beep_status = parse_json(report_msg->receive_payload);
                    pwm_task(beep_status);
                    break;
                case EN_MSG_REPORT:
                    mqtt_publish(report_topic, report_msg);
                    break;
                default:
                    break;
            }
            osal_kfree(report_msg);
        }
        osal_msleep(1000); // 1000等待连接成功
    }
    return ret;
}

/**
 * @brief 环境数据采集任务
 * @note 该任务负责：
 *       1. 初始化传感器
 *       2. 周期性读取温湿度数据
 *       3. 将数据写入消息队列
 */
void environment_task_entry(void)
{
    MQTT_msg *mqtt_msg;
    environment_msg aht_msg;
    mqtt_msg = osal_kmalloc(sizeof(MQTT_msg), 0);
    // 检查内存分配
    if (mqtt_msg == NULL) {
        printf("Memory allocation failed\r\n");
    }
    // aht20_init();
    // pwm_init();
    while (1) {
        aht20_test_task(&aht_msg);
        mqtt_msg->msg_type = EN_MSG_REPORT;
        if ((mqtt_msg != NULL) && (aht_msg.temperature != 0) && (aht_msg.humidity != 0)) {
            sprintf(mqtt_msg->temp, "%.2f", aht_msg.temperature);
            sprintf(mqtt_msg->humi, "%.2f", aht_msg.humidity);
            printf("temperature = %s, humidity= %s\r\n", mqtt_msg->temp, mqtt_msg->humi);
            uint32_t ret = osal_msg_queue_write_copy(g_msg_queue, mqtt_msg, sizeof(MQTT_msg), OSAL_WAIT_FOREVER);
            if (ret != 0) {
                printf("ret = %#x\r\n", ret);
                osal_kfree(mqtt_msg);
                break;
            }
        }
        osal_msleep(1000); // 1000ms读取数据
    }
    osal_kfree(mqtt_msg);
}

/**
 * @brief MQTT示例程序入口函数
 * @note 该函数负责：
 *       1. 创建消息队列
 *       2. 创建MQTT任务和环境数据采集任务
 *       3. 设置任务优先级
 */
static void mqtt_sample_entry(void)
{
    uint32_t ret;
    uapi_watchdog_disable();
    ret = osal_msg_queue_create("name", MSG_QUEUE_SIZE, &g_msg_queue, 0, MSG_MAX_LEN);
    if (ret != OSAL_SUCCESS) {
        printf("create queue failure!,error:%x\n", ret);
    }
    printf("create the queue success! queue_id = %d\n", g_msg_queue);
    osal_task *task_handle = NULL;
    osal_task *env_task_handle = NULL;
    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)mqtt_task, 0, "MqttDemoTask", MQTT_STA_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, MQTT_STA_TASK_PRIO);
        osal_kfree(task_handle);
    }
    env_task_handle =
        osal_kthread_create((osal_kthread_handler)environment_task_entry, 0, "EnvDemoTask", MQTT_STA_TASK_STACK_SIZE);
    if (env_task_handle != NULL) {
        osal_kthread_set_priority(env_task_handle, MQTT_STA_TASK_PRIO);
        osal_kfree(env_task_handle);
    }
    osal_kthread_unlock();
}

/* 入口点：通过app_run启动mqtt_sample_entry */
app_run(mqtt_sample_entry);