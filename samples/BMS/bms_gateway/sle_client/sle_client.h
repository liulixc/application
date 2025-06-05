/**
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2023-2023. All rights reserved.
 *
 * Description: SLE UART sample of client. \n
 *
 * History: \n
 * 2023-04-03, Create file. \n
 */
#ifndef SLE_UART_CLIENT_H
#define SLE_UART_CLIENT_H

#include "sle_ssap_client.h"



// ����豸ӳ�����ṹ
typedef struct {
    uint16_t conn_id;           // ����ID
    uint8_t device_index;       // ��g_env_msg�����е�����
    bool is_active;             // �豸�Ƿ��Ծ
    uint8_t device_mac[6];      // �豸MAC��ַ
    char cloud_device_id[64];   // ��Ϊ���豸ID (�� "680b91649314d11851158e8d_Battery05")
} bms_device_map_t;




// ������������
void sle_uart_client_init(ssapc_notification_callback notification_cb, 
                          ssapc_indication_callback indication_cb);
void sle_uart_notification_cb(uint8_t client_id, uint16_t conn_id, 
                             ssapc_handle_value_t *data, errcode_t status);
void sle_uart_indication_cb(uint8_t client_id, uint16_t conn_id, 
                           ssapc_handle_value_t *data, errcode_t status);
void sle_uart_start_scan(void);

// ��ȡ���Ͳ���
ssapc_write_param_t *get_g_sle_uart_send_param(void);

// ��ȡ��Ծ�豸����
uint8_t get_active_device_count(void);

#endif