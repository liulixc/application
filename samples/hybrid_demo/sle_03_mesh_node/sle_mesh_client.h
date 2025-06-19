/*
 * Copyright (c) dt-sir. All rights reserved.
 *
 * Description: SLE MESH Client module.
 */

#ifndef SLE_MESH_CLIENT_H
#define SLE_MESH_CLIENT_H

#include "sle_ssap_client.h"
#include "sle_common.h"
#include "sle_mesh_common.h"
#include "sle_device_discovery.h"
#include "sle_connection_manager.h"

/**
 * @brief 初始化Mesh客户端模块
 */
errcode_t sle_mesh_client_init(void);

/**
 * @brief 开始扫描潜在的父节点
 */
void start_scan(void);

/**
 * @brief 停止扫描
 */
void stop_scan(void);

/**
 * @brief 连接到指定的父节点地址
 * @param addr 目标父节点的地址
 */
errcode_t sle_mesh_client_connect(const sle_addr_t *addr);

/**
 * @brief 向父节点发送数据
 * @param data 要发送的数据指针
 * @param len  数据长度
 */
errcode_t sle_mesh_client_send_data(const uint8_t *data, uint16_t len);

/**
 * @brief 由主任务调用的回调，通知客户端与父节点的连接已断开
 */
void sle_mesh_client_parent_disconnected(void);

// Callbacks that will be registered in sle_mesh_callbacks.c
void sle_client_seek_result_cbk(sle_seek_result_info_t *seek_result);
void sle_client_seek_disable_cbk(errcode_t status);
void sle_client_connect_state_changed_cbk(uint16_t conn_id, const sle_addr_t *addr,
    sle_acb_state_t conn_state, sle_pair_state_t pair_state, sle_disc_reason_t disc_reason);
void sle_client_pair_complete_cbk(uint16_t conn_id, const sle_addr_t *addr, errcode_t status);

#endif 