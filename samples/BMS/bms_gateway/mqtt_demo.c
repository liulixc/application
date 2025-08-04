#include "mqtt_demo.h"

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


// ======================== 全局变量定义 ========================

volatile MQTTClient_deliveryToken deliveredtoken;  // 消息投递令牌
// 设备认证信息
char *g_username = "680b91649314d11851158e8d_Battery01"; // 设备ID
char *g_password = "50f670e657058bb33c23b92a633720a7fbbfba36f493f263c346b55bb2fb8bf3"; // 设备密码

#define MQTT_CLIENT_RESPONSE "$oc/devices/680b91649314d11851158e8d_Battery01/sys/commands/response/request_id=%s" // 命令响应topic

static MQTTClient client = NULL;                      // MQTT客户端实例
extern int MQTTClient_init(void);       // MQTT客户端初始化函数声明


char g_send_buffer[512] = {0}; // 发布数据缓冲区
char g_response_id[100] = {0}; // 保存命令id缓冲区

char g_response_buf[] =
    "{\"result_code\": 0,\"response_name\": \"battery\",\"paras\": {\"result\": \"success\"}}"; // 响应json

// 命令队列相关定义
#define MAX_CMD_QUEUE_SIZE 5
typedef struct {
    MQTT_msg cmd_msg;
    char response_id[100];
} cmd_queue_item_t;

volatile cmd_queue_item_t g_cmd_queue[MAX_CMD_QUEUE_SIZE];
volatile int g_cmd_queue_head = 0;
volatile int g_cmd_queue_tail = 0;
volatile int g_cmd_queue_count = 0;
extern int wifi_msg_flag; // WiFi配置修改标志


volatile environment_msg g_env_msg[MAX_BMS_DEVICES];


// ======================== 回调函数实现 ========================
void delivered(void *context, MQTTClient_deliveryToken dt)
{
    unused(context);
    printf("Message with token value %d delivery confirmed\r\n", dt);
    deliveredtoken = dt;
}

int msgArrved(void *context, char *topic_name, int topic_len, MQTTClient_message *message)
{
    unused(context);
    unused(topic_len);
    
    printf("[MQTT消息] 收到消息，主题: %s\n", topic_name);
    
    // 检查队列是否已满
    if (g_cmd_queue_count >= MAX_CMD_QUEUE_SIZE) {
        printf("[命令队列] 队列已满，丢弃命令\n");
        return 1;
    }
    
    // 获取队列尾部位置
    int tail_index = g_cmd_queue_tail;
    
    // 清空队列项
    memset((void*)&g_cmd_queue[tail_index], 0, sizeof(cmd_queue_item_t));
    
    // 安全拷贝payload内容
    if (message && message->payload && message->payloadlen > 0) {
        size_t copy_len = sizeof(g_cmd_queue[tail_index].cmd_msg.receive_payload) - 1;
        if ((size_t)message->payloadlen < copy_len) copy_len = message->payloadlen;
        memcpy(g_cmd_queue[tail_index].cmd_msg.receive_payload, message->payload, copy_len);
        g_cmd_queue[tail_index].cmd_msg.receive_payload[copy_len] = '\0';
    } else {
        g_cmd_queue[tail_index].cmd_msg.receive_payload[0] = '\0';
    }
    
    // 从topic中提取request_id
    char *request_id_pos = strstr(topic_name, "request_id=");
    if (request_id_pos != NULL) {
        request_id_pos += strlen("request_id=");
        size_t id_len = strlen(request_id_pos);
        if (id_len < sizeof(g_cmd_queue[tail_index].response_id)) {
            strcpy(g_cmd_queue[tail_index].response_id, request_id_pos);
            printf("[请求ID] 提取到request_id: %s\n", g_cmd_queue[tail_index].response_id);
        } else {
            printf("[请求ID] request_id过长,截断处理\n");
            strncpy(g_cmd_queue[tail_index].response_id, request_id_pos, sizeof(g_cmd_queue[tail_index].response_id) - 1);
            g_cmd_queue[tail_index].response_id[sizeof(g_cmd_queue[tail_index].response_id) - 1] = '\0';
        }
    } else {
        printf("[请求ID] 未找到request_id,使用默认值\n");
        strcpy(g_cmd_queue[tail_index].response_id, "default_request_id");
    }
    
    // 更新队列状态
    g_cmd_queue_tail = (g_cmd_queue_tail + 1) % MAX_CMD_QUEUE_SIZE;
    g_cmd_queue_count++;
    
    printf("[命令队列] 命令已入队，队列长度: %d,消息内容: %s\n", g_cmd_queue_count, g_cmd_queue[tail_index].cmd_msg.receive_payload);
    return 1;
}

void connlost(void *context, char *cause)
{
    unused(context);
    printf("mqtt_connection_lost() error, cause: %s\n", cause);
}

extern bool is_device_active[12]; // 设备活跃状态数组
// ======================== MQTT操作函数 ========================
int mqtt_publish_multi_device(const char *topic)
{
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;
    cJSON *root = cJSON_CreateObject();
    cJSON *devices = cJSON_CreateArray();
    
    // 遍历所有活跃设备
    for (int i = 2; i < 12; i++) {
        if (is_device_active[i]) {
            // 使用相同的索引
            environment_msg *env = &g_env_msg[i];
            
            // 创建单个设备的JSON
            cJSON *device = cJSON_CreateObject();
            // 使用设备ID
            char device_id[50];
            snprintf(device_id, sizeof(device_id), "680b91649314d11851158e8d_Battery%02d", i);
            cJSON_AddStringToObject(device, "device_id", device_id);
            
            cJSON *services = cJSON_CreateArray();
            cJSON *service = cJSON_CreateObject();
            cJSON_AddStringToObject(service, "service_id", "ws63");
            
            cJSON *props = cJSON_CreateObject();
            
            // 添加温度数组
            cJSON *temp_array = cJSON_CreateArray();
            for (int t = 0; t < 5; t++) {
                cJSON_AddItemToArray(temp_array, cJSON_CreateNumber(g_env_msg[i].temperature[t]));
            }
            cJSON_AddItemToObject(props, "temperature", temp_array);
            
            // 添加其他属性，确保格式与华为云期望的完全一致
            cJSON_AddNumberToObject(props, "current", g_env_msg[i].current);
            cJSON_AddNumberToObject(props, "total_voltage", g_env_msg[i].total_voltage);
            cJSON_AddNumberToObject(props, "SOC", g_env_msg[i].soc);
            
            // 添加电池电压数组
            cJSON *cell_array = cJSON_CreateArray();
            for (int c = 0; c < 12; c++) {
                cJSON_AddItemToArray(cell_array, cJSON_CreateNumber(g_env_msg[i].cell_voltages[c]));
            }
            cJSON_AddItemToObject(props, "cell_voltages", cell_array);
            
            // 添加节点层级和子节点数量
            cJSON_AddNumberToObject(props, "level", g_env_msg[i].level);
            cJSON_AddStringToObject(props, "child", g_env_msg[i].child);  // 改为字符串类型
            
            // 组装JSON
            cJSON_AddItemToObject(service, "properties", props);
            cJSON_AddItemToArray(services, service);
            cJSON_AddItemToObject(device, "services", services);
            cJSON_AddItemToArray(devices, device);
        }
    }
    
    cJSON_AddItemToObject(root, "devices", devices);
    char *json_str = cJSON_Print(root);
    
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

int mqtt_publish(const char *topic, const char *payload)
{
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;
    
    pubmsg.payload = (void*)payload;
    pubmsg.payloadlen = strlen(payload);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;
    
    int rc = MQTTClient_publishMessage(client, topic, &pubmsg, &token);
    if (rc == MQTTCLIENT_SUCCESS) {
        printf("[MQTT发布] 成功发布消息到主题: %s\n", topic);
        printf("[MQTT发布] 消息内容: %s\n", payload);
    } else {
        printf("[MQTT发布] 发布失败，错误码: %d\n", rc);
    }
    
    return rc;
}

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

net_type_t current_net = NET_TYPE_WIFI;

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
        printf("[网络切换] 断开4G,准备连接WiFi...\n");
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

int mqtt_task(void)
{
    
    app_uart_init_config();// 初始化4G串口
    int ret = 0;
    int loop_counter = 0; // 循环计数器，用于控制WiFi检查间隔
    
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

        // // 组合上报主题字符串
        // char *report_topic = combine_strings(3, "$oc/devices/", g_username, "/sys/properties/report");

        // 组合上报主题字符串
        char *gate_report_topic = combine_strings(3, "$oc/devices/", g_username, "/sys/gateway/sub_devices/properties/report");


        while (1) {
            // 检查WiFi配置是否发生变化
            if (wifi_msg_flag) {
                printf("[WiFi重连] 检测到WiFi配置变化,开始重连流程\n");
                if (switch_to_wifi(g_wifi_ssid, g_wifi_pwd) == 1) {
                        printf("[网络管理] 成功连接并切换到WiFi模式\\n");
                    } else {
                        printf("[网络管理] 连接WiFi失败,继续使用4G\\n");
                    }
                // 清除WiFi配置变化标志
                wifi_msg_flag = 0;
                printf("[WiFi重连] WiFi重连流程完成\n");
            }
            
            // 处理命令队列
            if (g_cmd_queue_count > 0) {
                // 获取队列头部命令
                int head_index = g_cmd_queue_head;
                cmd_queue_item_t current_cmd = g_cmd_queue[head_index];
                
                printf("[命令处理] 处理队列命令: %s\n", current_cmd.cmd_msg.receive_payload);
                
                // 检查是否为OTA升级命令
                if (strstr(current_cmd.cmd_msg.receive_payload, "\"command_name\":\"ota_upgrade\"") != NULL) {
                    printf("[OTA] 收到OTA升级命令，解析参数\n");
                    
                    // 解析JSON获取OTA配置参数
                    cJSON *json = cJSON_Parse(current_cmd.cmd_msg.receive_payload);
                    if (json != NULL) {
                        cJSON *paras = cJSON_GetObjectItem(json, "paras");
                         if (paras != NULL) {
                             cJSON *server_ip = cJSON_GetObjectItem(paras, "server_ip");
                             cJSON *server_port = cJSON_GetObjectItem(paras, "server_port");
                             cJSON *firmware_path = cJSON_GetObjectItem(paras, "firmware_path");
                             cJSON *device_id = cJSON_GetObjectItem(paras, "device_id");
                             
                             int ota_result = -1;
                             
                             // 提取参数，使用提供的值或默认值
                             const char *ip = (server_ip && cJSON_IsString(server_ip)) ? server_ip->valuestring : "1.13.92.135";
                             int port = (server_port && cJSON_IsNumber(server_port)) ? (int)server_port->valuedouble : 7999;
                             const char *path = (firmware_path && cJSON_IsString(firmware_path)) ? firmware_path->valuestring : "/api/firmware/download/test.bin";
                             const char *dev_id = (device_id && cJSON_IsString(device_id)) ? device_id->valuestring : "gateway_main";
                             
                             printf("[OTA] 使用配置: IP=%s, Port=%d, Path=%s, DeviceID=%s\n", ip, port, path, dev_id);
                             
                             ota_result = ota_task_start_with_config(ip, port, path, dev_id);
                            
                            // 发送响应
                             sprintf(g_send_buffer, MQTT_CLIENT_RESPONSE, current_cmd.response_id);
                             if (ota_result == 0) {
                                 printf("[OTA] OTA任务启动成功\n");
                                 char ota_response[] = "{\"result_code\": 0,\"response_name\": \"ota_upgrade\",\"paras\": {\"result\": \"ota_started\"}}";
                                 mqtt_publish(g_send_buffer, ota_response);
                             } else if (ota_result == -2) {
                                 printf("[OTA] 设备ID不匹配，此设备不需要升级\n");
                                 char ota_mismatch_response[] = "{\"result_code\": 0,\"response_name\": \"ota_upgrade\",\"paras\": {\"result\": \"device_id_mismatch\"}}";
                                 mqtt_publish(g_send_buffer, ota_mismatch_response);
                             } else {
                                 printf("[OTA] OTA任务启动失败\n");
                                 char ota_error_response[] = "{\"result_code\": 1,\"response_name\": \"ota_upgrade\",\"paras\": {\"result\": \"ota_start_failed\"}}";
                                 mqtt_publish(g_send_buffer, ota_error_response);
                             }
                        } else {
                            printf("[OTA] JSON解析失败：缺少paras字段\n");
                            sprintf(g_send_buffer, MQTT_CLIENT_RESPONSE, current_cmd.response_id);
                            char ota_error_response[] = "{\"result_code\": 1,\"response_name\": \"ota_upgrade\",\"paras\": {\"result\": \"invalid_params\"}}";
                            mqtt_publish(g_send_buffer, ota_error_response);
                        }
                        cJSON_Delete(json);
                    } else {
                        printf("[OTA] JSON解析失败\n");
                        sprintf(g_send_buffer, MQTT_CLIENT_RESPONSE, current_cmd.response_id);
                        char ota_error_response[] = "{\"result_code\": 1,\"response_name\": \"ota_upgrade\",\"paras\": {\"result\": \"json_parse_failed\"}}";
                        mqtt_publish(g_send_buffer, ota_error_response);
                    }
                } else {
                    // 其他命令直接下发给子设备
                    sle_gateway_send_command_to_children((uint8_t*)current_cmd.cmd_msg.receive_payload, strlen(current_cmd.cmd_msg.receive_payload));
                    
                    // 构建并发送响应
                    sprintf(g_send_buffer, MQTT_CLIENT_RESPONSE, current_cmd.response_id);
                    mqtt_publish(g_send_buffer, g_response_buf);
                }
                printf("[命令响应] 已发送响应到: %s\n", g_send_buffer);
                
                // 更新队列状态
                g_cmd_queue_head = (g_cmd_queue_head + 1) % MAX_CMD_QUEUE_SIZE;
                g_cmd_queue_count--;
                
                printf("[命令队列] 命令处理完成，剩余队列长度: %d\n", g_cmd_queue_count);
            }
            
            // 智能网络管理逻辑
            if (loop_counter % 10 == 0) {  // 每10秒检查一次网络状态
                if (current_net == NET_TYPE_4G) {
                    // 当前是4G模式，检查WiFi是否可用
                    if (switch_to_wifi(g_wifi_ssid, g_wifi_pwd) == 1) {
                        printf("[网络管理] 成功连接并切换到WiFi模式\\n");
                    } else {
                        printf("[网络管理] 连接WiFi失败,继续使用4G\\n");
                    }
                }
            }

            // 根据当前网络类型选择上报方式
            if (current_net == NET_TYPE_WIFI) {
                if (!check_wifi_status()) {
                        printf("[网络管理] WiFi发布失败且WiFi已断开,切换到4G模式\n");
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
                        printf("WiFi MQTT多设备发布成功,活跃设备数量:%d\r\n", active_count);
                    }
                } else {
                    printf("[MQTT] 跳过数据上报:无活跃BMS设备连接\r\n");
                }        
            } else if (current_net == NET_TYPE_4G) {
                // 调用封装的4G数据上报函数
                L610_PublishBMSDevices(gate_report_topic, (volatile void *)g_env_msg, is_device_active, get_active_device_count);
            }
            
            osal_msleep(500);
            loop_counter++;
        }
    }
    
    return ret;
}

// 任务相关配置
#define MQTT_STA_TASK_PRIO 17           // MQTT任务优先级
#define MQTT_STA_TASK_STACK_SIZE 0x2000 // MQTT任务栈大小
#define TIMEOUT 10000L                  // 超时时间：10秒

static void mqtt_sample_entry(void)
{
    uint32_t ret;
    // 禁用看门狗，防止开发阶段重启
    uapi_watchdog_disable();
    osal_task *task_handle = NULL;
    // 初始化全局结构体，防止野指针
    memset((void*)&g_env_msg, 0, sizeof(g_env_msg));
    memset((void*)&g_cmd_queue, 0, sizeof(g_cmd_queue));
    g_cmd_queue_head = 0;
    g_cmd_queue_tail = 0;
    g_cmd_queue_count = 0;
    MQTTClient_init(); // 只在入口初始化一次
    // 加锁，防止多线程冲突
    
    osal_kthread_lock();
    // 创建MQTT主任务
    task_handle = osal_kthread_create((osal_kthread_handler)mqtt_task, 0, "MqttDemoTask", MQTT_STA_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, MQTT_STA_TASK_PRIO);
        osal_kfree(task_handle);
    }
    osal_kthread_unlock();
}

// 程序入口点
app_run(mqtt_sample_entry);