#include "common_def.h"
#include "soc_osal.h"
#include "securec.h"
#include "product.h"
#include "bts_le_gap.h"
#include "uart.h"
#include "pinctrl.h"
#include "app_init.h"
#include "cJSON.h"
#include "mqtt_demo.h"
#include "wifi_connect.h"
#include "monitor.h"
#include "sle_client.h"
#include "mqtt_demo.h"
#include "nv_storage.h"
#include "nv_common_cfg.h"
#include "key_id.h"

#define UART_SIZE_DEFAULT 1024
#define MAX_BMS_DEVICES 2  // 设备数量上限

unsigned long g_msg_queue = 0;
unsigned int g_msg_rev_size = sizeof(msg_data_t);
/* 串口接收缓冲区大小 */
#define UART_RX_MAX 1024
uint8_t uart_rx_bufferNew[UART_RX_MAX];

char g_wifi_ssid[MAX_WIFI_SSID_LEN] = "QQ"; // 默认SSID
char g_wifi_pwd[MAX_WIFI_PASSWORD_LEN] = "tangyuan"; // 默认密码
int wifi_msg_flag = 0; // WiFi修改标志位

/* WiFi配置加载函数 */
static void load_wifi_config(void)
{
    wifi_config_t wifi_config = {0};
    uint32_t kvalue_length = sizeof(wifi_config_t);
    
    // 从NV存储读取WiFi配置
    errcode_t ret = uapi_nv_read(NV_ID_WIFI_CONFIG, kvalue_length, &kvalue_length, (uint8_t *)&wifi_config);
    if (ret == ERRCODE_SUCC && kvalue_length == sizeof(wifi_config_t)) {
        // 检查读取的数据是否有效（非空字符串）
        if (wifi_config.ssid[0] != '\0' && wifi_config.password[0] != '\0') {
            // 确保字符串以null结尾
            wifi_config.ssid[WIFI_SSID_MAX_LEN] = '\0';
            wifi_config.password[WIFI_PASSWORD_MAX_LEN] = '\0';
            
            // 复制到全局变量
            if (strcpy_s(g_wifi_ssid, MAX_WIFI_SSID_LEN, (char *)wifi_config.ssid) == EOK &&
                strcpy_s(g_wifi_pwd, MAX_WIFI_PASSWORD_LEN, (char *)wifi_config.password) == EOK) {
                printf("WiFi config loaded from NV - SSID: %s\r\n", g_wifi_ssid);
            } else {
                printf("Failed to copy WiFi config from NV\r\n");
            }
        } else {
            printf("Invalid WiFi config in NV, using defaults\r\n");
        }
    } else {
        printf("Failed to read WiFi config from NV (ret: 0x%x), using defaults\r\n", ret);
    }
}

/* WiFi配置保存函数 */
static void save_wifi_config(void)
{
    wifi_config_t wifi_config = {0};
    
    // 复制当前WiFi配置到结构体
    if (strcpy_s((char *)wifi_config.ssid, WIFI_SSID_MAX_LEN + 1, g_wifi_ssid) != EOK ||
        strcpy_s((char *)wifi_config.password, WIFI_PASSWORD_MAX_LEN + 1, g_wifi_pwd) != EOK) {
        printf("Failed to prepare WiFi config for saving\r\n");
        return;
    }
    
    // 写入NV存储
    errcode_t ret = uapi_nv_write(NV_ID_WIFI_CONFIG, (uint8_t *)&wifi_config, sizeof(wifi_config_t));
    if (ret == ERRCODE_SUCC) {
        printf("WiFi config saved to NV successfully\r\n");
    } else {
        printf("Failed to save WiFi config to NV (ret: 0x%x)\r\n", ret);
    }
}

// 外部变量声明
extern volatile environment_msg g_env_msg[MAX_BMS_DEVICES];

/* 串口接收回调 */
void sle_uart_client_read_handler(const void *buffer, uint16_t length, bool error)
{
    unused(error);
    msg_data_t msg_data = {0};
    void *buffer_cpy = osal_vmalloc(length);
    if (memcpy_s(buffer_cpy, length, buffer, length) != EOK) {
        osal_vfree(buffer_cpy);
        return;
    }
    msg_data.value = (uint8_t *)buffer_cpy;
    msg_data.value_len = length;
    osal_msg_queue_write_copy(g_msg_queue, (void *)&msg_data, g_msg_rev_size, 0);
    printf("monitor uart read handler, length: %d\r\n", length);
}
/* 串口初始化配置 */
void monitor_uart_init_config(void)
{
    uart_buffer_config_t uart_buffer_config;
    uapi_pin_set_mode(CONFIG_UART_TXD_PIN, CONFIG_UART_PIN_MODE);
    uapi_pin_set_mode(CONFIG_UART_RXD_PIN, CONFIG_UART_PIN_MODE);
    uart_attr_t attr = {
        .baud_rate = 115200, .data_bits = UART_DATA_BIT_8, .stop_bits = UART_STOP_BIT_1, .parity = UART_PARITY_NONE};
    uart_buffer_config.rx_buffer_size = UART_SIZE_DEFAULT;
    uart_buffer_config.rx_buffer = uart_rx_bufferNew;
    uart_pin_config_t pin_config = {.tx_pin = S_MGPIO0, .rx_pin = S_MGPIO1, .cts_pin = PIN_NONE, .rts_pin = PIN_NONE};
    uapi_uart_deinit(CONFIG_UART_ID);
    int res = uapi_uart_init(CONFIG_UART_ID, &pin_config, &attr, NULL, &uart_buffer_config);
    if (res != 0) {
        printf("uart init failed res = %02x\r\n", res);
    }
    if (uapi_uart_register_rx_callback(CONFIG_UART_ID, UART_RX_CONDITION_MASK_IDLE, 3, sle_uart_client_read_handler) ==
        ERRCODE_SUCC) {
        printf("uart%d int mode register receive callback succ!\r\n", CONFIG_UART_ID);
    }
}

// 发送数据包
uint32_t monitor_uart_send_buff(uint8_t *str, uint16_t len)
{
    uint32_t ret = 0;
    ret = uapi_uart_write(CONFIG_UART_ID, str, len, 0xffffffff);
    if (ret != 0) {
        printf("send lenth:%d\n", ret);
    }
    return ret;
}

// 外部声明
extern volatile environment_msg g_env_msg[];
extern uint8_t get_active_device_count(void);
extern bool is_device_active[12]; // 设备活跃状态数组
extern net_type_t current_net; // 当前网络状态
static void *monitorTX_task(char *arg)
{
    unused(arg);
    while (1) {
        // 检查是否有活跃的BMS设备连接
        uint8_t active_count = get_active_device_count();
        if (active_count > 0) {
            // 创建根JSON对象
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
                snprintf(device_id, sizeof(device_id), "_Battery%02d", i);
                cJSON_AddStringToObject(device, "device_id", device_id);
                
                cJSON *services = cJSON_CreateArray();
                cJSON *service = cJSON_CreateObject();
                cJSON_AddStringToObject(service, "service_id", "ws63");
                
                cJSON *props = cJSON_CreateObject();
                
                // 添加温度数组
                cJSON *temp_array = cJSON_CreateArray();
                char temp_buffer[16];
                for (int t = 0; t < 5; t++) {
                    snprintf(temp_buffer, sizeof(temp_buffer), "%.2f", g_env_msg[i].temperature[t]/1000.0f);
                    cJSON_AddItemToArray(temp_array, cJSON_CreateNumber(atof(temp_buffer)));
                }
                cJSON_AddItemToObject(props, "temperature", temp_array);
                
                // 添加其他属性
                // 使用格式化方式限制小数点后两位
                char num_buffer[16];
                // 格式化电流，限制为小数点后两位
                snprintf(num_buffer, sizeof(num_buffer), "%.2f", g_env_msg[i].current/10000.0f);
                cJSON_AddNumberToObject(props, "current", atof(num_buffer));
                
                // 格式化总电压，限制为小数点后两位
                snprintf(num_buffer, sizeof(num_buffer), "%.2f", g_env_msg[i].total_voltage/10000.0f);
                cJSON_AddNumberToObject(props, "total_voltage", atof(num_buffer));
                
                
                cJSON_AddNumberToObject(props, "SOC", g_env_msg[i].soc);
                cJSON_AddNumberToObject(props, "iswifi", current_net);
                
                // 添加电池电压数组，每个电压值限制为小数点后两位
                cJSON *cell_array = cJSON_CreateArray();
                for (int c = 0; c < 12; c++) {
                    // 格式化单体电压，限制为小数点后两位
                    snprintf(num_buffer, sizeof(num_buffer), "%.2f", g_env_msg[i].cell_voltages[c]/10000.0f);
                    cJSON_AddItemToArray(cell_array, cJSON_CreateNumber(atof(num_buffer)));
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
            char *json_str = cJSON_PrintUnformatted(root);
            
            // 发送JSON数据到串口屏
            monitor_uart_send_buff((uint8_t *)json_str, strlen(json_str));
            printf("monitor:%s\r\n", json_str);
                
            cJSON_free(json_str);
        
            
            // 释放资源
            cJSON_Delete(root);
        } else {
            // printf("bms null\r\n");
        }

        static int loop_counter = 0;
        loop_counter++;
        if (loop_counter % 3 == 0) {
            for (int i = 2; i < 12; i++) {
                is_device_active[i] = false;
            }

            loop_counter = 0; // 重置计数器
        }
        osal_msleep(3000); // 每隔1秒发送一次数据
    }
    return NULL;
}

static void *monitor_task(char *arg)
{
    unused(arg);
    monitor_uart_init_config();
    while (1) {
        msg_data_t msg_data = {0};
        int msg_ret = osal_msg_queue_read_copy(g_msg_queue, &msg_data, &g_msg_rev_size, OSAL_WAIT_FOREVER);
        if (msg_ret != OSAL_SUCCESS) {
            printf("msg queue read copy fail.");
            if (msg_data.value != NULL) {
                osal_vfree(msg_data.value);
            }
        }        
        
        if (msg_data.value != NULL) {
            //在这个地方处理从串口屏接收到的消息
            // 使用cJSON解析JSON数据
            
            // 添加字符串结束符
            char *json_str = (char *)osal_vmalloc(msg_data.value_len + 1);
            if (json_str == NULL) {
                printf("Failed to allocate memory for JSON string\r\n");
                osal_vfree(msg_data.value);
                continue;
            }
            
            if (memcpy_s(json_str, msg_data.value_len + 1, msg_data.value, msg_data.value_len) != EOK) {
                printf("Failed to copy JSON data\r\n");
                osal_vfree(json_str);
                osal_vfree(msg_data.value);
                continue;
            }
            json_str[msg_data.value_len] = '\0';
            
            printf("Received UART message: %s\r\n", json_str);
            
            // 解析JSON数据
            cJSON *json = cJSON_Parse(json_str);
            if (json == NULL) {
                printf("JSON parse failed\r\n");
                osal_vfree(json_str);
                osal_vfree(msg_data.value);
                continue;
            }
            
            // 解析cmd字段
            cJSON *cmd = cJSON_GetObjectItem(json, "command");
            if (cJSON_IsNumber(cmd)) {
                int cmd_value = cmd->valueint;
                switch (cmd_value) {
                    case MONITOR_CMD_TYPE_WIFI: // 设置WiFi信息
                    {
                        printf("Processing WiFi configuration command\r\n");
                        
                        // 解析SSID
                        cJSON *ssid = cJSON_GetObjectItem(json, "SSID");
                        if (!cJSON_IsString(ssid)) {
                            printf("Invalid or missing SSID field\r\n");
                        }
                        
                        // 解析密码
                        cJSON *password = cJSON_GetObjectItem(json, "password");
                        if (!cJSON_IsString(password)) {
                            printf("Invalid or missing password field\r\n");
                            break;
                        }
                        
                        // 检查长度限制
                        if (strlen(ssid->valuestring) >= MAX_WIFI_SSID_LEN ||
                            strlen(password->valuestring) >= MAX_WIFI_PASSWORD_LEN) {
                            printf("WiFi credentials too long\r\n");
                            break;
                        }

                        // 判断是否有变化
                        int need_update = strcmp(g_wifi_ssid, ssid->valuestring) != 0 || strcmp(g_wifi_pwd, password->valuestring ) != 0;
                        if (need_update) {
                            if (strcpy_s(g_wifi_ssid, MAX_WIFI_SSID_LEN, ssid->valuestring) == EOK &&
                                strcpy_s(g_wifi_pwd, MAX_WIFI_PASSWORD_LEN, password->valuestring) == EOK) {
                                wifi_msg_flag = 1;
                                printf("WiFi config updated - SSID: %s, Password: %s\r\n", g_wifi_ssid, g_wifi_pwd);
                                // 保存WiFi配置到NV存储
                                save_wifi_config();
                            } else {
                                printf("Failed to update WiFi configuration\r\n");
                            }
                        } else {
                            printf("WiFi配置未变化, 不触发重连\r\n");
                        }
                        break;
                    }
                    
                    default:
                        break;
                }
            } else {
                printf("Invalid or missing cmd field\r\n");
            }
            
            // 清理资源
            cJSON_Delete(json);
            osal_vfree(json_str);
            osal_vfree(msg_data.value);
        }
    }
    return NULL;
}



static void monitor_entry(void)
{
    // 加载WiFi配置
    load_wifi_config();
    
    osal_task *Monitor_task_handle = NULL;
    osal_task *task_handle = NULL;
    osal_kthread_lock();
    int ret = osal_msg_queue_create("monitor", g_msg_rev_size, &g_msg_queue, 0, g_msg_rev_size);
    if (ret != OSAL_SUCCESS) {
        printf("create monitor queue failure!,error:%x\n", ret);
    }

    Monitor_task_handle =
        osal_kthread_create((osal_kthread_handler)monitor_task, 0, "monitor_task", MONITOR_STACK_SIZE);
    if (Monitor_task_handle != NULL) {
        osal_kthread_set_priority(Monitor_task_handle, MONITOR_TASK_PRIO);
        osal_kfree(Monitor_task_handle);
    }

    task_handle = osal_kthread_create((osal_kthread_handler)monitorTX_task, 0, "monitorTX_task", MONITOR_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, MONITOR_TASK_PRIO);
        osal_kfree(task_handle);
    }

    osal_kthread_unlock();
}

app_run(monitor_entry);

