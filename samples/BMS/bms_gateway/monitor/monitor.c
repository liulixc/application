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

#define UART_SIZE_DEFAULT 1024
#define MAX_BMS_DEVICES 2  // �豸��������

unsigned long g_msg_queue = 0;
unsigned int g_msg_rev_size = sizeof(msg_data_t);
/* ���ڽ��ջ�������С */
#define UART_RX_MAX 1024
uint8_t uart_rx_bufferNew[UART_RX_MAX];

char g_wifi_ssid[MAX_WIFI_SSID_LEN] = "QQ"; // Ĭ��SSID
char g_wifi_pwd[MAX_WIFI_PASSWORD_LEN] = "tangyuan"; // Ĭ������
int wifi_msg_flag = 0; // WiFi�޸ı�־λ

// �ⲿ��������
extern volatile environment_msg g_env_msg[MAX_BMS_DEVICES];
extern bms_device_map_t g_bms_device_map[MAX_BMS_DEVICES];

/* ���ڽ��ջص� */
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
/* ���ڳ�ʼ������ */
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

// �������ݰ�
static uint32_t uart_send_buff(uint8_t *str, uint16_t len)
{
    uint32_t ret = 0;
    ret = uapi_uart_write(CONFIG_UART_ID, str, len, 0xffffffff);
    if (ret != 0) {
        printf("send lenth:%d\n", ret);
    }
    return ret;
}

// �ⲿ����
extern volatile environment_msg g_env_msg[];
extern uint8_t get_active_device_count(void);
extern bool is_device_active[12]; // �豸��Ծ״̬����
extern net_type_t current_net; // ��ǰ����״̬
static void *monitorTX_task(char *arg)
{
    unused(arg);
    
    
    
    while (1) {
        // ����Ƿ��л�Ծ��BMS�豸����
        uint8_t active_count = get_active_device_count();
        if (active_count > 0) {
            // ������JSON����
            cJSON *root = cJSON_CreateObject();
            cJSON *devices = cJSON_CreateArray();
            
            
            // �������л�Ծ�豸
        for (int i = 2; i < 12; i++) {
            if (is_device_active[i]) {
                // ʹ����ͬ������
                environment_msg *env = &g_env_msg[i];
                
                // ���������豸��JSON
                cJSON *device = cJSON_CreateObject();
                // ʹ���豸ID
                char device_id[50];
                snprintf(device_id, sizeof(device_id), "_Battery%02d", i);
                cJSON_AddStringToObject(device, "device_id", device_id);
                
                cJSON *services = cJSON_CreateArray();
                cJSON *service = cJSON_CreateObject();
                cJSON_AddStringToObject(service, "service_id", "ws63");
                
                cJSON *props = cJSON_CreateObject();
                
                // ����¶�����
                cJSON *temp_array = cJSON_CreateArray();
                char temp_buffer[16];
                for (int t = 0; t < 5; t++) {
                    snprintf(temp_buffer, sizeof(temp_buffer), "%.2f", g_env_msg[i].temperature[t]/1000.0f);
                    cJSON_AddItemToArray(temp_array, cJSON_CreateNumber(atof(temp_buffer)));
                }
                cJSON_AddItemToObject(props, "temperature", temp_array);
                
                // �����������
                // ʹ�ø�ʽ����ʽ����С�������λ
                char num_buffer[16];
                // ��ʽ������������ΪС�������λ
                snprintf(num_buffer, sizeof(num_buffer), "%.2f", g_env_msg[i].current/10000.0f);
                cJSON_AddNumberToObject(props, "current", atof(num_buffer));
                
                // ��ʽ���ܵ�ѹ������ΪС�������λ
                snprintf(num_buffer, sizeof(num_buffer), "%.2f", g_env_msg[i].total_voltage/10000.0f);
                cJSON_AddNumberToObject(props, "total_voltage", atof(num_buffer));
                
                
                cJSON_AddNumberToObject(props, "SOC", g_env_msg[i].soc);
                cJSON_AddNumberToObject(props, "iswifi", current_net);
                
                // ��ӵ�ص�ѹ���飬ÿ����ѹֵ����ΪС�������λ
                cJSON *cell_array = cJSON_CreateArray();
                for (int c = 0; c < 12; c++) {
                    // ��ʽ�������ѹ������ΪС�������λ
                    snprintf(num_buffer, sizeof(num_buffer), "%.2f", g_env_msg[i].cell_voltages[c]/10000.0f);
                    cJSON_AddItemToArray(cell_array, cJSON_CreateNumber(atof(num_buffer)));
                }
                cJSON_AddItemToObject(props, "cell_voltages", cell_array);
                
                // ��װJSON
                cJSON_AddItemToObject(service, "properties", props);
                cJSON_AddItemToArray(services, service);
                cJSON_AddItemToObject(device, "services", services);
                cJSON_AddItemToArray(devices, device);
            }
        }

            cJSON_AddItemToObject(root, "devices", devices);
            char *json_str = cJSON_PrintUnformatted(root);
            
            // ����JSON���ݵ�������
            uart_send_buff((uint8_t *)json_str, strlen(json_str));
            printf("monitor:%s\r\n", json_str);
                
            cJSON_free(json_str);
        
            
            // �ͷ���Դ
            cJSON_Delete(root);
        } else {
            printf("bms null\r\n");
        }

        static int loop_counter = 0;
        loop_counter++;
        if (loop_counter % 5 == 0) {
            for (int i = 2; i < 12; i++) {
                is_device_active[i] = false;
            }
            osal_msleep(500);
            loop_counter = 0; // ���ü�����
        }
        osal_msleep(1000); // ÿ��1�뷢��һ������
    }
    return NULL;
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
            //������ط�����Ӵ��������յ�����Ϣ
            // ʹ��cJSON����JSON����
            
            // ����ַ���������
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
            
            // ����JSON����
            cJSON *json = cJSON_Parse(json_str);
            if (json == NULL) {
                printf("JSON parse failed\r\n");
                osal_vfree(json_str);
                osal_vfree(msg_data.value);
                continue;
            }
            
            // ����cmd�ֶ�
            cJSON *cmd = cJSON_GetObjectItem(json, "command");
            if (cJSON_IsNumber(cmd)) {
                int cmd_value = cmd->valueint;
                
                switch (cmd_value) {
                    case MONITOR_CMD_TYPE_WIFI: // ����WiFi��Ϣ
                    {
                        printf("Processing WiFi configuration command\r\n");
                        
                        // ����SSID
                        cJSON *ssid = cJSON_GetObjectItem(json, "SSID");
                        if (!cJSON_IsString(ssid)) {
                            printf("Invalid or missing SSID field\r\n");
                        }
                        
                        // ��������
                        cJSON *password = cJSON_GetObjectItem(json, "password");
                        if (!cJSON_IsString(password)) {
                            printf("Invalid or missing password field\r\n");
                            break;
                        }
                        
                        // ��鳤������
                        if (strlen(ssid->valuestring) >= MAX_WIFI_SSID_LEN ||
                            strlen(password->valuestring) >= MAX_WIFI_PASSWORD_LEN) {
                            printf("WiFi credentials too long\r\n");
                            break;
                        }

                        // �ж��Ƿ��б仯
                        int need_update = strcmp(g_wifi_ssid, ssid->valuestring) != 0 || strcmp(g_wifi_pwd, password->valuestring ) != 0;
                        if (need_update) {
                            if (strcpy_s(g_wifi_ssid, MAX_WIFI_SSID_LEN, ssid->valuestring) == EOK &&
                                strcpy_s(g_wifi_pwd, MAX_WIFI_PASSWORD_LEN, password->valuestring) == EOK) {
                                wifi_msg_flag = 1;
                                printf("WiFi config updated - SSID: %s, Password: %s\r\n", g_wifi_ssid, g_wifi_pwd);
                                // TODO: ����WiFi���ø��º�������������WiFi
                            } else {
                                printf("Failed to update WiFi configuration\r\n");
                            }
                        } else {
                            printf("WiFi����δ�仯, ����������\r\n");
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
            }
            
            // ������Դ
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

