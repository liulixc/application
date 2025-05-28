/*
 * Copyright (c) 2024 HiSilicon Technologies CO., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "common_def.h"
#include "soc_osal.h"
#include "securec.h"
#include "product.h"
#include "bts_le_gap.h"
#include "uart.h"
#include "pinctrl.h"
#include "sle_device_discovery.h"
#include "sle_connection_manager.h"
#include "sle_client.h"
#include "app_init.h"
#include "cJSON.h"
#include "mqtt_demo.h"

#define SLE_MTU_SIZE_DEFAULT 512
#define SLE_SEEK_INTERVAL_DEFAULT 100
#define SLE_SEEK_WINDOW_DEFAULT 100
#define UUID_16BIT_LEN 2
#define UUID_128BIT_LEN 16

#define SLE_SERVER_NAME "sle_test"

static unsigned long g_msg_queue = 0;
unsigned int g_msg_rev_size = sizeof(msg_data_t);
/* 串口接收缓冲区大小 */
#define UART_RX_MAX 1024
static uint8_t uart_rx_buffer[UART_RX_MAX];

static ssapc_find_service_result_t g_sle_find_service_result = {0};
static sle_announce_seek_callbacks_t g_sle_uart_seek_cbk = {0};
static sle_connection_callbacks_t g_sle_uart_connect_cbk = {0};
static ssapc_callbacks_t g_sle_uart_ssapc_cbk = {0};
static sle_addr_t g_sle_remote_addr = {0};
ssapc_write_param_t g_sle_send_param = {0};
uint16_t g_sle_uart_conn_id = 0;

extern volatile environment_msg g_env_msg; // 全局环境消息变量

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

static void app_uart_init_config(void);
/* 开启扫描 */
void sle_start_scan(void)
{
    sle_seek_param_t param = {0};
    param.own_addr_type = 0;
    param.filter_duplicates = 0;
    param.seek_filter_policy = 0;
    param.seek_phys = 1;
    param.seek_type[0] = 1;
    param.seek_interval[0] = SLE_SEEK_INTERVAL_DEFAULT;
    param.seek_window[0] = SLE_SEEK_WINDOW_DEFAULT;
    sle_set_seek_param(&param);
    sle_start_seek();
}
/* 星闪协议栈使能回调 */
static void sle_client_sle_enable_cbk(errcode_t status)
{
    unused(status);
    printf("sle enable.\r\n");
    sle_start_scan();
}
/* 扫描使能回调函数 */
static void sle_client_seek_enable_cbk(errcode_t status)
{
    if (status != 0) {
        printf("[%s] status error\r\n", __FUNCTION__);
    }
}
/* 返回扫描结果回调 */
static void sle_client_seek_result_info_cbk(sle_seek_result_info_t *seek_result_data)
{
    printf("[seek_result_info_cbk] scan data : "); // 打印扫描到的设备名称 hex
    for (uint8_t i = 0; i < seek_result_data->data_length; i++) {
        printf("0x%X ", seek_result_data->data[i]);
    }
    printf("\r\n");
    if (seek_result_data == NULL) {
        printf("status error\r\n");
    } else if (strstr((const char *)seek_result_data->data, SLE_SERVER_NAME) != NULL) { // 名称对比成功
        memcpy_s(&g_sle_remote_addr, sizeof(sle_addr_t), &seek_result_data->addr,
                 sizeof(sle_addr_t)); // 扫描到目标设备，将目标设备的名字拷贝到g_sle_remote_addr
        sle_stop_seek();              // 停止扫描
    }
}
/* 扫描关闭回调函数 */
static void sle_client_seek_disable_cbk(errcode_t status)
{
    if (status != 0) {
        printf("[%s] status error = %x\r\n", __FUNCTION__, status);
    } else {
        sle_remove_paired_remote_device(&g_sle_remote_addr); // 关闭扫描后，先删除之前的配对
        sle_connect_remote_device(&g_sle_remote_addr);       // 发送连接请求
    }
}
/* 扫描注册初始化函数 */
static void sle_client_seek_cbk_register(void)
{
    g_sle_uart_seek_cbk.sle_enable_cb = sle_client_sle_enable_cbk;
    g_sle_uart_seek_cbk.seek_enable_cb = sle_client_seek_enable_cbk;
    g_sle_uart_seek_cbk.seek_result_cb = sle_client_seek_result_info_cbk;
    g_sle_uart_seek_cbk.seek_disable_cb = sle_client_seek_disable_cbk;
    sle_announce_seek_register_callbacks(&g_sle_uart_seek_cbk);
}
/* 连接状态改变回调 */
static void sle_client_connect_state_changed_cbk(uint16_t conn_id,
                                                 const sle_addr_t *addr,
                                                 sle_acb_state_t conn_state,
                                                 sle_pair_state_t pair_state,
                                                 sle_disc_reason_t disc_reason)
{
    unused(addr);
    unused(pair_state);
    printf("[%s] disc_reason:0x%x\r\n", __FUNCTION__, disc_reason);
    g_sle_uart_conn_id = conn_id;
    if (conn_state == SLE_ACB_STATE_CONNECTED) {
        printf(" SLE_ACB_STATE_CONNECTED\r\n");
        if (pair_state == SLE_PAIR_NONE) {
            sle_pair_remote_device(&g_sle_remote_addr);
        }
        printf(" sle_low_latency_rx_enable \r\n");
    } else if (conn_state == SLE_ACB_STATE_NONE) {
        printf(" SLE_ACB_STATE_NONE\r\n");
    } else if (conn_state == SLE_ACB_STATE_DISCONNECTED) {
        printf(" SLE_ACB_STATE_DISCONNECTED\r\n");
        sle_remove_paired_remote_device(&g_sle_remote_addr);
        sle_start_scan();
    } else {
        printf(" status error\r\n");
    }
}
/* 配对完成回调 */
void sle_client_pair_complete_cbk(uint16_t conn_id, const sle_addr_t *addr, errcode_t status)
{
    printf("[%s] conn_id:%d, state:%d,addr:%02x***%02x%02x\n", __FUNCTION__, conn_id, status, addr->addr[0],
           addr->addr[4], addr->addr[5]); /* 0 14 15: addr */
    if (status == 0) {
        ssap_exchange_info_t info = {0};
        info.mtu_size = SLE_MTU_SIZE_DEFAULT;
        info.version = 1;
        ssapc_exchange_info_req(0, g_sle_uart_conn_id, &info);
    }
}
/* 连接注册初始化函数 */
static void sle_client_connect_cbk_register(void)
{
    g_sle_uart_connect_cbk.connect_state_changed_cb = sle_client_connect_state_changed_cbk;
    g_sle_uart_connect_cbk.pair_complete_cb = sle_client_pair_complete_cbk;
    sle_connection_register_callbacks(&g_sle_uart_connect_cbk);
}

/* 更新mtu回调 */
static void sle_client_exchange_info_cbk(uint8_t client_id,
                                         uint16_t conn_id,
                                         ssap_exchange_info_t *param,
                                         errcode_t status)
{
    printf("[%s] pair complete client id:%d status:%d\r\n", __FUNCTION__, client_id, status);
    printf("[%s] mtu size: %d, version: %d.\r\n", __FUNCTION__, param->mtu_size, param->version);
    ssapc_find_structure_param_t find_param = {0};
    find_param.type = SSAP_FIND_TYPE_PROPERTY;
    find_param.start_hdl = 1;
    find_param.end_hdl = 0xFFFF;
    ssapc_find_structure(0, conn_id, &find_param);
}
/* 发现服务回调 */
static void sle_client_find_structure_cbk(uint8_t client_id,
                                          uint16_t conn_id,
                                          ssapc_find_service_result_t *service,
                                          errcode_t status)
{
    printf("[%s] client: %d conn_id:%d status: %d \r\n", __FUNCTION__, client_id, conn_id, status);
    printf("[%s] find structure start_hdl:[0x%02x], end_hdl:[0x%02x], uuid len:%d\r\n", __FUNCTION__,
           service->start_hdl, service->end_hdl, service->uuid.len);
    g_sle_find_service_result.start_hdl = service->start_hdl;
    g_sle_find_service_result.end_hdl = service->end_hdl;
    memcpy_s(&g_sle_find_service_result.uuid, sizeof(sle_uuid_t), &service->uuid, sizeof(sle_uuid_t));
}
/* 发现特征回调 */
static void sle_client_find_property_cbk(uint8_t client_id,
                                         uint16_t conn_id,
                                         ssapc_find_property_result_t *property,
                                         errcode_t status)
{
    printf(
        "[%s] client id: %d, conn id: %d, operate ind: %d, "
        "descriptors count: %d status:%d property->handle %d\r\n",
        __FUNCTION__, client_id, conn_id, property->operate_indication, property->descriptors_count, status,
        property->handle);
    g_sle_send_param.handle = property->handle;
    g_sle_send_param.type = SSAP_PROPERTY_TYPE_VALUE;
}
/* 发现特征完成回调 */
static void sle_client_find_structure_cmp_cbk(uint8_t client_id,
                                              uint16_t conn_id,
                                              ssapc_find_structure_result_t *structure_result,
                                              errcode_t status)
{
    unused(conn_id);
    printf("[%s] client id:%d status:%d type:%d uuid len:%d \r\n", __FUNCTION__, client_id, status,
           structure_result->type, structure_result->uuid.len);
}
/* 收到写响应回调 */
static void sle_client_write_cfm_cbk(uint8_t client_id,
                                     uint16_t conn_id,
                                     ssapc_write_result_t *write_result,
                                     errcode_t status)
{
    printf("[%s] conn_id:%d client id:%d status:%d handle:%02x type:%02x\r\n", __FUNCTION__, conn_id, client_id, status,
           write_result->handle, write_result->type);
}
/* 收到通知回调 */
void sle_notification_cbk(uint8_t client_id, uint16_t conn_id, ssapc_handle_value_t *data, errcode_t status)
{
    unused(client_id);
    unused(conn_id);
    unused(status);
    
    if (data == NULL || data->data == NULL || data->data_len == 0) {
        return;
    }
    // 简单打印接收的数据长度，不打印完整内容以避免日志缓冲区问题
    printf("Received notification data, length: %d\r\n", data->data_len);
    char *json_str = NULL;
    json_str = (char *)osal_vmalloc(data->data_len + 1);
    if (json_str == NULL) {
        printf("Failed to allocate memory for JSON string\r\n");
        return;
    }
    // 复制数据到临时缓冲区并添加字符串结束符
    if (memcpy_s(json_str, data->data_len + 1, data->data, data->data_len) != EOK) {
        osal_vfree(json_str);
        printf("Failed to copy JSON data\r\n");
        return;
    }
    json_str[data->data_len] = '\0';
    
    // 使用cJSON解析JSON数据
    cJSON *json = NULL;
    json = cJSON_Parse(json_str);
    if (json == NULL) {
        printf("JSON parse failed\r\n");
        osal_vfree(json_str);
        return;
    }
    
    // 解析total_voltage
    cJSON *total_voltage = cJSON_GetObjectItem(json, "total_voltage");
    if (cJSON_IsNumber(total_voltage)) {
        g_env_msg.total_voltage = total_voltage->valueint;
        g_env_msg.current = total_voltage->valueint;
    } else {
        printf("Invalid or missing total_voltage field\r\n");
    }


    
    // 解析cell_voltages数组
    cJSON *cell_voltages = cJSON_GetObjectItem(json, "cell_voltages");
    if (cJSON_IsArray(cell_voltages)) {
        int size = cJSON_GetArraySize(cell_voltages);
        int max_cells = sizeof(g_env_msg.cell_voltages)/sizeof(g_env_msg.cell_voltages[0]);
        for (int i = 0; i < size && i < max_cells; i++) {
            cJSON *item = cJSON_GetArrayItem(cell_voltages, i);
            if (cJSON_IsNumber(item)) {
                g_env_msg.cell_voltages[i] = item->valueint;
            }
        }
    } else {
        printf("Invalid or missing cell_voltages array\r\n");
    }
    
    // 解析temperature数组
    cJSON *temperature = cJSON_GetObjectItem(json, "temperature");
    if (cJSON_IsArray(temperature)) {
        int size = cJSON_GetArraySize(temperature);
        int max_temps = sizeof(g_env_msg.temperature)/sizeof(g_env_msg.temperature[0]);
        for (int i = 0; i < size && i < max_temps; i++) {
            cJSON *item = cJSON_GetArrayItem(temperature, i);
            if (cJSON_IsNumber(item)) {
                g_env_msg.temperature[i] = item->valueint;
            }
        }
    } else {
        printf("Invalid or missing temperature array\r\n");
    }
    
    // // 解析current
    // cJSON *current = cJSON_GetObjectItem(json, "current");
    // if (cJSON_IsNumber(current)) {
    //     g_env_msg.current = current->valueint;  // 正确使用current变量
    // } else {
    //     printf("Invalid or missing current field\r\n");
    // }
    
    // 确保所有数据都安全地访问
    printf("Parsed data: total_voltage=%u, current=%u\r\n", 
           g_env_msg.total_voltage, g_env_msg.current);
    
    
    // 清理资源
    cJSON_Delete(json);
    osal_vfree(json_str);
}
/* 收到指示回调 */
void sle_indication_cbk(uint8_t client_id, uint16_t conn_id, ssapc_handle_value_t *data, errcode_t status)
{
    unused(client_id);
    unused(conn_id);
    unused(status);
    printf("recv len:%d data: ", data->data_len);
    for (uint16_t i = 0; i < data->data_len; i++) {
        printf("%c", data->data[i]);
    }
    printf("\r\n");
}

static void sle_client_ssapc_cbk_register(void)
{
    g_sle_uart_ssapc_cbk.exchange_info_cb = sle_client_exchange_info_cbk;
    g_sle_uart_ssapc_cbk.find_structure_cb = sle_client_find_structure_cbk;
    g_sle_uart_ssapc_cbk.ssapc_find_property_cbk = sle_client_find_property_cbk;
    g_sle_uart_ssapc_cbk.find_structure_cmp_cb = sle_client_find_structure_cmp_cbk;
    g_sle_uart_ssapc_cbk.write_cfm_cb = sle_client_write_cfm_cbk;
    g_sle_uart_ssapc_cbk.notification_cb = sle_notification_cbk;
    g_sle_uart_ssapc_cbk.indication_cb = sle_indication_cbk;
    ssapc_register_callbacks(&g_sle_uart_ssapc_cbk);
}

void sle_client_init(void)
{
    osal_msleep(500); /* 500: 延时 */
    printf("sle enable.\r\n");
    sle_client_seek_cbk_register();
    sle_client_connect_cbk_register();
    sle_client_ssapc_cbk_register();
    if (enable_sle() != ERRCODE_SUCC) {
        printf("sle enbale fail !\r\n");
    }
}


static void *sle_client_task(char *arg)
{
    unused(arg);
    // app_uart_init_config();
    sle_client_init();
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
            ssapc_write_param_t *sle_uart_send_param = &g_sle_send_param;
            sle_uart_send_param->data_len = msg_data.value_len;
            sle_uart_send_param->data = msg_data.value;
            ssapc_write_cmd(0, g_sle_uart_conn_id, sle_uart_send_param);
        }
    }
    return NULL;
}
static void sle_client_entry(void)
{
    osal_task *task_handle = NULL;
    osal_kthread_lock();
    int ret = osal_msg_queue_create("sle_msg", g_msg_rev_size, &g_msg_queue, 0, g_msg_rev_size);
    if (ret != OSAL_SUCCESS) {
        printf("create queue failure!,error:%x\n", ret);
    }

    task_handle =
        osal_kthread_create((osal_kthread_handler)sle_client_task, 0, "sle_client_task", SLE_SERVER_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, SLE_SERVER_TASK_PRIO);
        osal_kfree(task_handle);
    }
    osal_kthread_unlock();
}

/* Run the sle_uart_entry. */
app_run(sle_client_entry);

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
static void app_uart_init_config(void)
{
    uart_buffer_config_t uart_buffer_config;
    uapi_pin_set_mode(CONFIG_UART_TXD_PIN, CONFIG_UART_PIN_MODE);
    uapi_pin_set_mode(CONFIG_UART_RXD_PIN, CONFIG_UART_PIN_MODE);
    uart_attr_t attr = {
        .baud_rate = 115200, .data_bits = UART_DATA_BIT_8, .stop_bits = UART_STOP_BIT_1, .parity = UART_PARITY_NONE};
    uart_buffer_config.rx_buffer_size = SLE_MTU_SIZE_DEFAULT;
    uart_buffer_config.rx_buffer = uart_rx_buffer;
    uart_pin_config_t pin_config = {.tx_pin = S_MGPIO0, .rx_pin = S_MGPIO1, .cts_pin = PIN_NONE, .rts_pin = PIN_NONE};
    uapi_uart_deinit(CONFIG_UART_ID);
    int res = uapi_uart_init(CONFIG_UART_ID, &pin_config, &attr, NULL, &uart_buffer_config);
    if (res != 0) {
        printf("uart init failed res = %02x\r\n", res);
    }
    if (uapi_uart_register_rx_callback(CONFIG_UART_ID, UART_RX_CONDITION_MASK_IDLE, 1, sle_uart_client_read_handler) ==
        ERRCODE_SUCC) {
        printf("uart%d int mode register receive callback succ!\r\n", CONFIG_UART_ID);
    }
}