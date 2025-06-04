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

#define UART_SIZE_DEFAULT 1024

unsigned long g_msg_queue = 0;
unsigned int g_msg_rev_size = sizeof(msg_data_t);
/* 串口接收缓冲区大小 */
#define UART_RX_MAX 1024
uint8_t uart_rx_bufferNew[UART_RX_MAX];

char g_wifi_ssid[MAX_WIFI_SSID_LEN] = "QQ"; // 默认SSID
char g_wifi_pwd[MAX_WIFI_PASSWORD_LEN] = "tangyuan"; // 默认密码

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
}
/* 串口初始化配置 */
static app_uart_init_config(void)
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
static uint32_t uart_send_buff(uint8_t *str, uint16_t len)
{
    uint32_t ret = 0;
    ret = uapi_uart_write(CONFIG_UART_ID, str, len, 0xffffffff);
    if (ret != 0) {
        printf("send lenth:%d\n", ret);
    }
    return ret;
}


static void *monitor_task(char *arg)
{
    unused(arg);
    app_uart_init_config();
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
                            char response[] = "{\"status\":\"error\",\"msg\":\"invalid_ssid\"}\n";
                            uart_send_buff((uint8_t *)response, strlen(response));
                            break;
                        }
                        
                        // 解析密码
                        cJSON *password = cJSON_GetObjectItem(json, "password");
                        if (!cJSON_IsString(password)) {
                            printf("Invalid or missing password field\r\n");
                            char response[] = "{\"status\":\"error\",\"msg\":\"invalid_password\"}\n";
                            uart_send_buff((uint8_t *)response, strlen(response));
                            break;
                        }
                        
                        // 检查长度限制
                        if (strlen(ssid->valuestring) >= MAX_WIFI_SSID_LEN ||
                            strlen(password->valuestring) >= MAX_WIFI_PASSWORD_LEN) {
                            printf("WiFi credentials too long\r\n");
                            char response[] = "{\"status\":\"error\",\"msg\":\"credentials_too_long\"}\n";
                            uart_send_buff((uint8_t *)response, strlen(response));
                            break;
                        }
                        
                        // 更新全局WiFi配置
                        if (strcpy_s(g_wifi_ssid, MAX_WIFI_SSID_LEN, ssid->valuestring) == EOK &&
                            strcpy_s(g_wifi_pwd, MAX_WIFI_PASSWORD_LEN, password->valuestring) == EOK) {
                            
                            printf("WiFi config updated - SSID: %s, Password: %s\r\n", 
                                   g_wifi_ssid, g_wifi_pwd);
                            
                            // 发送成功响应
                            char response[] = "{\"status\":\"success\",\"msg\":\"wifi_config_updated\"}\n";
                            uart_send_buff((uint8_t *)response, strlen(response));
                            
                            // 可以在这里触发WiFi重连接等操作
                            // TODO: 调用WiFi配置更新函数，重新连接WiFi
                            
                        } else {
                            printf("Failed to update WiFi configuration\r\n");
                            char response[] = "{\"status\":\"error\",\"msg\":\"update_failed\"}\n";
                            uart_send_buff((uint8_t *)response, strlen(response));
                        }
                        break;
                    }
                    
                    default:
                        // printf("Unknown command: %d\r\n", cmd_value);
                        // char response[] = "{\"status\":\"error\",\"msg\":\"unknown_command\"}\n";
                        // uart_send_buff((uint8_t *)response, strlen(response));
                        break;
                }
            } else {
                printf("Invalid or missing cmd field\r\n");
                char response[] = "{\"status\":\"error\",\"msg\":\"invalid_cmd\"}\n";
                uart_send_buff((uint8_t *)response, strlen(response));
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
    osal_task *Monitor_task_handle = NULL;
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
    osal_kthread_unlock();
}

app_run(monitor_entry);

