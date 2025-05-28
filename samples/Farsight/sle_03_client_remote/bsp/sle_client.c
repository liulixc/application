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

#include "sle_client.h"
#include "bsp_st7789_4line.h"
#define SLE_MTU_SIZE_DEFAULT 512
#define SLE_SEEK_INTERVAL_DEFAULT 100
#define SLE_SEEK_WINDOW_DEFAULT 100
#define UUID_16BIT_LEN 2
#define UUID_128BIT_LEN 16

#define SLE_SERVER_NAME "sle_test"

ssapc_find_service_result_t g_sle_find_service_result = {0};
sle_announce_seek_callbacks_t g_sle_uart_seek_cbk = {0};
sle_connection_callbacks_t g_sle_uart_connect_cbk = {0};
ssapc_callbacks_t g_sle_uart_ssapc_cbk = {0};
sle_addr_t g_sle_remote_addr = {0};
ssapc_write_param_t g_sle_send_param = {0};
uint16_t g_sle_uart_conn_id = 0;

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
void sle_client_sle_enable_cbk(errcode_t status)
{
    unused(status);
    printf("sle enable.\r\n");
    sle_start_scan();
}
/* 扫描使能回调函数 */
void sle_client_seek_enable_cbk(errcode_t status)
{
    if (status != 0) {
        printf("[%s] status error\r\n", __FUNCTION__);
    }
}
/* 返回扫描结果回调 */
void sle_client_seek_result_info_cbk(sle_seek_result_info_t *seek_result_data)
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
void sle_client_seek_disable_cbk(errcode_t status)
{
    if (status != 0) {
        printf("[%s] status error = %x\r\n", __FUNCTION__, status);
    } else {
        sle_remove_paired_remote_device(&g_sle_remote_addr); // 关闭扫描后，先删除之前的配对
        sle_connect_remote_device(&g_sle_remote_addr);       // 发送连接请求
    }
}
/* 扫描注册初始化函数 */
void sle_client_seek_cbk_register(void)
{
    g_sle_uart_seek_cbk.sle_enable_cb = sle_client_sle_enable_cbk;
    g_sle_uart_seek_cbk.seek_enable_cb = sle_client_seek_enable_cbk;
    g_sle_uart_seek_cbk.seek_result_cb = sle_client_seek_result_info_cbk;
    g_sle_uart_seek_cbk.seek_disable_cb = sle_client_seek_disable_cbk;
    sle_announce_seek_register_callbacks(&g_sle_uart_seek_cbk);
}
/* 连接状态改变回调 */
void sle_client_connect_state_changed_cbk(uint16_t conn_id,
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
void sle_client_connect_cbk_register(void)
{
    g_sle_uart_connect_cbk.connect_state_changed_cb = sle_client_connect_state_changed_cbk;
    g_sle_uart_connect_cbk.pair_complete_cb = sle_client_pair_complete_cbk;
    sle_connection_register_callbacks(&g_sle_uart_connect_cbk);
}
/* 更新mtu回调 */
void sle_client_exchange_info_cbk(uint8_t client_id, uint16_t conn_id, ssap_exchange_info_t *param, errcode_t status)
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
void sle_client_find_structure_cbk(uint8_t client_id,
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
void sle_client_find_property_cbk(uint8_t client_id,
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
void sle_client_find_structure_cmp_cbk(uint8_t client_id,
                                       uint16_t conn_id,
                                       ssapc_find_structure_result_t *structure_result,
                                       errcode_t status)
{
    unused(conn_id);
    printf("[%s] client id:%d status:%d type:%d uuid len:%d \r\n", __FUNCTION__, client_id, status,
           structure_result->type, structure_result->uuid.len);
}
/* 收到写响应回调 */
void sle_client_write_cfm_cbk(uint8_t client_id, uint16_t conn_id, ssapc_write_result_t *write_result, errcode_t status)
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
    printf("recv len:%d data: ", data->data_len);
    for (uint16_t i = 0; i < data->data_len; i++) {
        printf("%c", data->data[i]);
    }
    memcpy(sle_recv, data->data, data->data_len);
    sle_recv_flag = 1;
}
/* 收到指示回调 */
void sle_indication_cbk(uint8_t client_id, uint16_t conn_id, ssapc_handle_value_t *data, errcode_t status)
{
    unused(client_id);
    unused(conn_id);
    unused(status);
    printf("indication len:%d data: ", data->data_len);
    for (uint16_t i = 0; i < data->data_len; i++) {
        printf("%c", data->data[i]);
    }
}

void sle_client_ssapc_cbk_register(void)
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
