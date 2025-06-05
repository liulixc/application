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



// 添加设备映射管理结构
typedef struct {
    uint16_t conn_id;           // 连接ID
    uint8_t device_index;       // 在g_env_msg数组中的索引
    bool is_active;             // 设备是否活跃
    uint8_t device_mac[6];      // 设备MAC地址
    char cloud_device_id[64];   // 华为云设备ID (如 "680b91649314d11851158e8d_Battery05")
} bms_device_map_t;




// 公共函数声明
void sle_uart_client_init(ssapc_notification_callback notification_cb, 
                          ssapc_indication_callback indication_cb);
void sle_uart_notification_cb(uint8_t client_id, uint16_t conn_id, 
                             ssapc_handle_value_t *data, errcode_t status);
void sle_uart_indication_cb(uint8_t client_id, uint16_t conn_id, 
                           ssapc_handle_value_t *data, errcode_t status);
void sle_uart_start_scan(void);

// 获取发送参数
ssapc_write_param_t *get_g_sle_uart_send_param(void);

// 获取活跃设备数量
uint8_t get_active_device_count(void);

#endif