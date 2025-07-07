/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2022. All rights reserved.
 *
 * Description: SLE private service register sample of client.
 */

/**
 * @defgroup SLE UUID CLIENT API
 * @ingroup
 * @{
 */

 #ifndef SLE_CLIENT_ADV_H
 #define SLE_CLIENT_ADV_H
 
 #include "sle_ssap_client.h"
 #include "sle_common.h"
 
 // 子节点连接信息管理结构体
 typedef struct {
     uint16_t conn_id;       // 连接ID
     bool is_active;         // 是否活跃
     uint8_t mac[6];         // 子节点的MAC地址
     uint16_t write_handle;  // 为该子节点存储的可写句柄
 } child_node_info_t;
 
 
 // void sle_set_server_name(char *name);
 void sle_hybridc_init(void);
 // int sle_hybridc_send_data(uint8_t *data, uint8_t length);
 
 // sle_addr_t *sle_get_remote_server_addr(void);
 // uint8_t sle_get_num_remote_server_addr(void);
 // uint8_t sle_hybridc_is_service_found(uint16_t conn_id);
 
 void sle_set_hybridc_addr(void);
 void sle_hybridc_adopt_orphan(uint16_t conn_id, uint8_t parent_level);
 uint8_t get_active_children_count(void);
 void sle_hybridc_disconnect_all_children(void);
 
 void sle_start_scan();
 
 sle_addr_t* sle_get_remote_server_addrs(void);
 uint8_t sle_get_remote_server_count(void);
 
 #endif