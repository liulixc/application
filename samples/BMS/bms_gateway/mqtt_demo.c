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

// 引入MQTT和操作系统相关头文件
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
#include "cjson_demo.h"
#include "cJSON.h"  // 包含cJSON库头文件
#include "mqtt_demo.h"
#include "l610.h"
#include "sle_client.h"  // 包含SLE客户端头文件，提供设备映射结构定义
#include "monitor.h"

// ======================== 配置参数 ========================

// MQTT服务器连接地址（华为云IoT平台）
#define ADDRESS "tcp://2502007d6f.st1.iotda-device.cn-north-4.myhuaweicloud.com"
// 客户端ID，用于唯一标识设备
#define CLIENTID "680b91649314d11851158e8d_Battery01_0_0_2025042603"
// 服务质量等级：1表示至少发送一次
#define QOS 1

// // WiFi配置信息
extern char g_wifi_ssid[MAX_WIFI_SSID_LEN]; // 默认SSID
extern char g_wifi_pwd[MAX_WIFI_PASSWORD_LEN]; // 默认密码


// 任务相关配置
#define MQTT_STA_TASK_PRIO 17           // MQTT任务优先级
#define MQTT_STA_TASK_STACK_SIZE 0x2000 // MQTT任务栈大小
#define TIMEOUT 10000L                  // 超时时间：10秒

// 最大支持8个BMS设备已在sle_client.h中定义

// ======================== 全局变量定义 ========================

/**
 * @brief 全局变量定义
 */
volatile MQTTClient_deliveryToken deliveredtoken;  // 消息投递令牌
// 设备认证信息
char *g_username = "680b91649314d11851158e8d_Battery01"; // 设备ID
char *g_password = "50f670e657058bb33c23b92a633720a7fbbfba36f493f263c346b55bb2fb8bf3"; // 设备密码
static MQTTClient client = NULL;                      // MQTT客户端实例
extern int MQTTClient_init(void);       // MQTT客户端初始化函数声明


volatile MQTT_msg g_cmd_msg;        // 全局命令消息变量
volatile int g_cmd_msg_flag = 0;    // 命令消息标志
extern int wifi_msg_flag; // WiFi配置修改标志

// 最大支持的BMS设备数
#define MAX_BMS_DEVICES 8

extern bms_device_map_t g_bms_device_map[MAX_BMS_DEVICES];
volatile environment_msg g_env_msg[MAX_BMS_DEVICES];


// ======================== 回调函数实现 ========================

/**
 * @brief 消息投递回调函数
 * @param context 上下文信息（未使用）
 * @param dt 投递令牌
 * @details 当消息成功投递到MQTT服务器时被调用
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
    // 直接写入全局命令变量
    memset((void*)&g_cmd_msg, 0, sizeof(MQTT_msg));
    // 安全拷贝payload内容，防止悬挂指针
    if (message && message->payload && message->payloadlen > 0) {
        size_t copy_len = sizeof(g_cmd_msg.receive_payload) - 1;
        if ((size_t)message->payloadlen < copy_len) copy_len = message->payloadlen;
        memcpy(g_cmd_msg.receive_payload, message->payload, copy_len);
        g_cmd_msg.receive_payload[copy_len] = '\0';
    } else {
        g_cmd_msg.receive_payload[0] = '\0';
    }
    g_cmd_msg_flag = 1; // 标记有新命令
    printf("mqtt_message_arrive() success, the topic is %s, the payload is %s \n", topic_name, g_cmd_msg.receive_payload);
    return 1;
}

/**
 * @brief MQTT连接断开回调函数
 * @param context 上下文信息（未使用）
 * @param cause 断开连接的原因
 * @details 当MQTT连接丢失时被调用
 */
void connlost(void *context, char *cause)
{
    unused(context);
    printf("mqtt_connection_lost() error, cause: %s\n", cause);
}

// ======================== MQTT操作函数 ========================
int mqtt_publish_multi_device(const char *topic)
{
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;
    cJSON *root = cJSON_CreateObject();
    cJSON *devices = cJSON_CreateArray();
    
    // 遍历所有活跃设备
    for (int i = 0; i < MAX_BMS_DEVICES; i++) {
        if (g_bms_device_map[i].is_active) {
            int idx = g_bms_device_map[i].device_index;
            
            // 创建单个设备的JSON
            cJSON *device = cJSON_CreateObject();
            cJSON_AddStringToObject(device, "device_id", g_bms_device_map[i].cloud_device_id);
            
            cJSON *services = cJSON_CreateArray();
            cJSON *service = cJSON_CreateObject();
            cJSON_AddStringToObject(service, "service_id", "ws63");
            
            cJSON *props = cJSON_CreateObject();
            
            // 添加温度数组
            cJSON *temp_array = cJSON_CreateArray();
            for (int t = 0; t < 5; t++) {
                cJSON_AddItemToArray(temp_array, cJSON_CreateNumber(g_env_msg[idx].temperature[t]));
            }
            cJSON_AddItemToObject(props, "temperature", temp_array);
            
            // 添加其他属性，确保格式与华为云期望的完全一致
            cJSON_AddNumberToObject(props, "current", g_env_msg[idx].current);
            cJSON_AddNumberToObject(props, "total_voltage", g_env_msg[idx].total_voltage);
            cJSON_AddBoolToObject(props, "Switch", false);
            
            // 添加电池电压数组
            cJSON *cell_array = cJSON_CreateArray();
            for (int c = 0; c < 12; c++) {
                cJSON_AddItemToArray(cell_array, cJSON_CreateNumber(g_env_msg[idx].cell_voltages[c]));
            }
            cJSON_AddItemToObject(props, "cell_voltages", cell_array);
            
            // 组装JSON
            cJSON_AddItemToObject(service, "properties", props);
            cJSON_AddItemToArray(services, service);
            cJSON_AddItemToObject(device, "services", services);
            cJSON_AddItemToArray(devices, device);
        }
    }
    
    cJSON_AddItemToObject(root, "devices", devices);
    char *json_str = cJSON_Print(root);
    
    printf("Publishing multi-device data: %s\n", json_str);
    
    // 发布消息
    pubmsg.payload = json_str;
    pubmsg.payloadlen = strlen(json_str);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;
    
    int rc = MQTTClient_publishMessage(client, topic, &pubmsg, &token);
    if (rc == MQTTCLIENT_SUCCESS) {
        printf("成功发布多设备数据到华为云\n");
    } else {
        printf("发布多设备数据失败，错误码：%d\n", rc);
    }
    
    // 释放资源
    cJSON_Delete(root);
    free(json_str);
    
    return rc;
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
 * @brief 连接MQTT服务器
 * @return MQTTCLIENT_SUCCESS表示成功，-1表示失败
 * @note 该函数会初始化MQTT客户端，设置连接参数并建立连接
 *       同时会注册消息回调函数用于处理收到的消息
 */
int mqtt_connect(void)
{
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    printf("start mqtt sync subscribe...\r\n");
    errcode_t ret = MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    if (ret != MQTTCLIENT_SUCCESS) {
        osal_printk("Failed to create MQTT client,return code %d\r\n", ret);
        return ERRCODE_FAIL;
    }
    conn_opts.keepAliveInterval = 120; // 保持间隔为120秒，每120秒发送一次消息
    conn_opts.cleansession = 1;
    if (g_username != NULL) {
        conn_opts.username = g_username;
        conn_opts.password = g_password;
    }
    // 设置回调函数
    MQTTClient_setCallbacks(client, NULL, connlost, msgArrved, delivered);

    // 尝试连接到MQTT代理服务器
    if ((ret = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        osal_printk("[MQTT_Task] : Failed to connect ,return code %d\r\n", ret);
        MQTTClient_destroy(&client);           // 连接失败时销毁客户端资源
        return ERRCODE_FAIL;
    }
    printf("Connected to MQTT brocker\r\n");
    return MQTTCLIENT_SUCCESS;
}

// ======================== 网络切换流程 ========================

static net_type_t current_net = NET_TYPE_WIFI;

// 切换到WiFi
int switch_to_wifi(const char *ssid, const char *psk)
{
    // 切换前彻底释放旧的MQTT client，防止资源泄漏
    if (client != NULL) {
        // 解绑回调，防止回调悬挂
        MQTTClient_setCallbacks(client, NULL, NULL, NULL, NULL);
        MQTTClient_disconnect(client, 1000);
        MQTTClient_destroy(&client);
        client = NULL;
        osal_msleep(500); // 等待资源彻底释放
    }

    wifi_disconnect(); // 确保断开WiFi连接

    printf("[网络切换] 连接WiFi: SSID=%s\\n", ssid);
    if (wifi_connect(ssid, psk) == 0) {
        // WiFi连接成功后，建立MQTT连接
        if (mqtt_connect() == 0) {
            // 重新订阅命令topic
            char *cmd_topic = combine_strings(3, "$oc/devices/", g_username, "/sys/commands/#");
            if (cmd_topic) {
                mqtt_subscribe(cmd_topic);
                free(cmd_topic);
            }
            printf("[网络切换] MQTT连接和订阅成功\n");
        } else {
            printf("[网络切换] MQTT连接失败\n");
        }
        if (current_net == NET_TYPE_4G) {
        printf("[网络切换] 断开4G，准备连接WiFi...\n");
        L610_Detach(1);         // 断开4G
        }
        current_net = NET_TYPE_WIFI;
        return 1;
    } else {
        printf("[网络切换] WiFi连接失败\n");
        return 0;
    }
}

// 切换到4G
void switch_to_4g(void)
{
    if (current_net == NET_TYPE_WIFI) {
        printf("[网络切换] 断开WiFi，准备连接4G...\n");
        wifi_disconnect();      // 断开WiFi
    }
    printf("[网络切换] 连接4G\n");
    L610_Attach(1, 1);         // 连接4G
    L610_HuaweiCloudConnect(
        "117.78.5.125", // ip
        "1883",         // port
        "680b91649314d11851158e8d_Battery01", // clientid
        "12345678",     // password
        60,              // keepalive
        0                // cleanSession
    );
    current_net = NET_TYPE_4G;
    printf("[网络切换] 已切换到4G\n");
}

// ======================== 任务实现 ========================

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
    app_uart_init_config();// 初始化4G串口
    int ret = 0;
    char *beep_status = NULL;
    int loop_counter = 0; // 循环计数器，用于控制WiFi检查间隔
    int wifi_retry_counter = 0; // WiFi重试计数器
    
    // 连接WiFi
    if (wifi_connect(g_wifi_ssid, g_wifi_pwd) != 0) {
        printf("wifi connect failed\\n");
        // WiFi连接失败，切换到4G
        switch_to_4g();
    }else{
        // 连接MQTT服务器
        ret = mqtt_connect();
        if (ret != 0) {
            printf("connect failed, result %d\n", ret);
        }
        osal_msleep(1000); // 等待连接成功
        // 组合命令主题字符串
        char *cmd_topic = combine_strings(3, "$oc/devices/", g_username, "/sys/commands/#");
        if (cmd_topic) {
            ret = mqtt_subscribe(cmd_topic);
            free(cmd_topic);
        } else {
            printf("combine_strings failed for cmd_topic\n");
        }
    }
    
    // 组合上报主题字符串
    char *report_topic = combine_strings(3, "$oc/devices/", g_username, "/sys/properties/report");
    if (!report_topic) {
        printf("combine_strings failed for report_topic\n");
        return -1;
    }

    // 组合上报主题字符串
    char *gate_report_topic = combine_strings(3, "$oc/devices/", g_username, "/sys/gateway/sub_devices/properties/report");
    if (!report_topic) {
        printf("combine_strings failed for report_topic\n");
        return -1;
    }
    

    while (1) {
        // 检查WiFi配置是否发生变化
        if (wifi_msg_flag) {
            printf("[WiFi重连] 检测到WiFi配置变化，开始重连流程\n");
            if (switch_to_wifi(g_wifi_ssid, g_wifi_pwd) == 1) {
                    printf("[网络管理] 成功连接并切换到WiFi模式\\n");
                } else {
                    printf("[网络管理] 连接WiFi失败，继续使用4G\\n");
                }
            // 清除WiFi配置变化标志
            wifi_msg_flag = 0;
            printf("[WiFi重连] WiFi重连流程完成\n");
        }
        
        // 处理下发命令
        if (g_cmd_msg_flag) {
            if (g_cmd_msg.receive_payload[0] != '\0') {
                beep_status = parse_json(g_cmd_msg.receive_payload);
            } else {
                printf("Warning: receive_payload is empty, skip parse_json\n");
            }
            g_cmd_msg_flag = 0;
        }
        
        // 智能网络管理逻辑
        if (loop_counter % 10 == 0) {  // 每10秒检查一次网络状态
            if (current_net == NET_TYPE_4G) {
                // 当前是4G模式，检查WiFi是否可用
                if (switch_to_wifi(g_wifi_ssid, g_wifi_pwd) == 1) {
                    printf("[网络管理] 成功连接并切换到WiFi模式\\n");
                } else {
                    printf("[网络管理] 连接WiFi失败，继续使用4G\\n");
                }
            }
        }
        
        // 根据当前网络类型选择上报方式
        if (current_net == NET_TYPE_WIFI) {
            if (!check_wifi_status()) {
                    printf("[网络管理] WiFi发布失败且WiFi已断开，切换到4G模式\n");
                    switch_to_4g();
                    continue; // 跳过本次循环，等待4G连接
                }
            
            // 检查是否有活跃的BMS设备连接
            uint8_t active_count = get_active_device_count();
            if (active_count > 0) {
                // WiFi网络使用MQTT Client上报
                if (mqtt_publish_multi_device(gate_report_topic) != MQTTCLIENT_SUCCESS) {
                    printf("WiFi MQTT多设备发布失败\r\n");
                } else {
                    printf("WiFi MQTT多设备发布成功，活跃设备数量：%d\r\n", active_count);
                }
            } else {
                printf("[MQTT] 跳过数据上报：无活跃BMS设备连接\r\n");
            }        
        } else if (current_net == NET_TYPE_4G) {
            // 检查是否有活跃的BMS设备连接
            uint8_t active_count = get_active_device_count();
            if (active_count > 0) {
                // 4G网络使用L610模块上报多设备数据
                cJSON *root = cJSON_CreateObject();
                cJSON *devices = cJSON_CreateArray();
                
                // 遍历所有活跃设备
                for (int i = 0; i < MAX_BMS_DEVICES; i++) {
                    if (g_bms_device_map[i].is_active) {
                        int idx = g_bms_device_map[i].device_index;
                        
                        // 创建单个设备的JSON
                        cJSON *device = cJSON_CreateObject();
                        cJSON_AddStringToObject(device, "device_id", g_bms_device_map[i].cloud_device_id);
                        
                        cJSON *services = cJSON_CreateArray();
                        cJSON *service = cJSON_CreateObject();
                        cJSON_AddStringToObject(service, "service_id", "ws63");
                        
                        cJSON *props = cJSON_CreateObject();
                        
                        // 添加温度数组
                        cJSON *temp_array = cJSON_CreateArray();
                        for (int t = 0; t < 5; t++) {
                            cJSON_AddItemToArray(temp_array, cJSON_CreateNumber(g_env_msg[idx].temperature[t]));
                        }
                        cJSON_AddItemToObject(props, "temperature", temp_array);
                        
                        // 添加其他属性
                        cJSON_AddNumberToObject(props, "current", g_env_msg[idx].current);
                        cJSON_AddNumberToObject(props, "total_voltage", g_env_msg[idx].total_voltage);
                        cJSON_AddBoolToObject(props, "Switch", false);
                        
                        // 添加电池电压数组
                        cJSON *cell_array = cJSON_CreateArray();
                        for (int c = 0; c < 12; c++) {
                            cJSON_AddItemToArray(cell_array, cJSON_CreateNumber(g_env_msg[idx].cell_voltages[c]));
                        }
                        cJSON_AddItemToObject(props, "cell_voltages", cell_array);
                        
                        // 组装JSON
                        cJSON_AddItemToObject(service, "properties", props);
                        cJSON_AddItemToArray(services, service);
                        cJSON_AddItemToObject(device, "services", services);
                        cJSON_AddItemToArray(devices, device);
                    }
                }
                
                cJSON_AddItemToObject(root, "devices", devices);
                char *json_str = cJSON_Print(root);
                
                // 使用L610上报到华为云
                char *l610_topic = combine_strings(3, "$oc/devices/", g_username, "/sys/gateway/sub_devices/properties/report");
                if (l610_topic && json_str) {
                    L610_HuaweiCloudReport(l610_topic, json_str);
                    printf("[HUAWEI CLOUD] 4G多设备上报成功，活跃设备数量：%d\r\n", active_count);
                    free(l610_topic);
                } else {
                    printf("[HUAWEI CLOUD] 4G多设备上报失败：topic或payload生成失败\r\n");
                }
                
                // 释放资源
                cJSON_Delete(root);
                if (json_str) free(json_str);
            } else {
                printf("[L610] 跳过数据上报：无活跃BMS设备连接\r\n");
            }
        } else {
            printf("Warning: report_topic is NULL or network type unknown, skip publish\n");
        }

        osal_msleep(1000);
        loop_counter++;
    }
    
    return ret;
}

// ======================== MQTT示例程序入口函数 ========================
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
    // 禁用看门狗，防止开发阶段重启
    uapi_watchdog_disable();
    osal_task *task_handle = NULL;
    // 初始化全局结构体，防止野指针
    memset((void*)&g_env_msg, 0, sizeof(g_env_msg));
    memset((void*)&g_cmd_msg, 0, sizeof(g_cmd_msg));
    MQTTClient_init(); // 只在入口初始化一次
    // 加锁，防止多线程冲突
    
    osal_kthread_lock();
    // 创建MQTT主任务
    task_handle = osal_kthread_create((osal_kthread_handler)mqtt_task, 0, "MqttDemoTask", MQTT_STA_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, MQTT_STA_TASK_PRIO);
        osal_kfree(task_handle);
    }
    // 解锁
    osal_kthread_unlock();
}

/* 入口点：通过app_run启动mqtt_sample_entry */
app_run(mqtt_sample_entry);