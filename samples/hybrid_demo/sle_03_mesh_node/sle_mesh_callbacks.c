/**
 * Copyright (c) dt-sir. All rights reserved.
 * Description: 统一回调注册与分发.
 */
#include "soc_osal.h"
#include "sle_errcode.h"
#include "sle_connection_manager.h"
#include "sle_device_discovery.h"
#include "sle_common.h"
#include "errcode.h"
#include "sle_mesh_common.h"
#include <string.h>

// 客户端角色的回调函数 (来自 sle_mesh_client.c)
extern void sle_client_seek_result_cbk(sle_seek_result_info_t *seek_result_data);
extern void sle_client_seek_disable_cbk(errcode_t status);
extern void sle_client_connect_state_changed_cbk(uint16_t conn_id, const sle_addr_t *addr,
    sle_acb_state_t conn_state, sle_pair_state_t pair_state, sle_disc_reason_t disc_reason);
extern void sle_client_pair_complete_cbk(uint16_t conn_id, const sle_addr_t *addr, errcode_t status);

// 服务端角色的回调函数 (来自 sle_mesh_server.c)
extern void sle_server_announce_enable_cbk(uint32_t announce_id, errcode_t status);
extern void sle_server_announce_disable_cbk(uint32_t announce_id, errcode_t status);
extern void sle_server_announce_terminal_cbk(uint32_t announce_id);
extern void sle_server_connect_state_changed_cbk(uint16_t conn_id, const sle_addr_t *addr,
    sle_acb_state_t conn_state, sle_pair_state_t pair_state, sle_disc_reason_t disc_reason);
extern void sle_server_pair_complete_cbk(uint16_t conn_id, const sle_addr_t *addr, errcode_t status);

// 主任务模块的全局变量 (来自 sle_mesh_node.c)
extern mesh_neighbor_info_t g_parent_node;
extern mesh_node_role_t g_mesh_node_role;
extern volatile mesh_node_state_t g_current_state;
extern void sle_mesh_node_enable_cbk(errcode_t status);
extern void sle_mesh_node_disable_cbk(errcode_t status);

// --- 统一回调与分发逻辑 ---

static void sle_enable_cbk(errcode_t status)
{
    sle_mesh_node_enable_cbk(status);
}

static void sle_disable_cbk(errcode_t status)
{
    sle_mesh_node_disable_cbk(status);
}

// 连接状态变化的中央分发器
static void connect_state_changed_cbk(uint16_t conn_id, const sle_addr_t *addr,
    sle_acb_state_t conn_state, sle_pair_state_t pair_state, sle_disc_reason_t disc_reason)
{
    if (g_mesh_node_role == NODE_ROLE_GATEWAY) {
        sle_server_connect_state_changed_cbk(conn_id, addr, conn_state, pair_state, disc_reason);
        return;
    }

    // If we are a node and we were in the process of connecting, this must be the parent.
    if (g_current_state == MESH_NODE_STATE_CONNECTING && conn_state == SLE_ACB_STATE_CONNECTED) {
         sle_client_connect_state_changed_cbk(conn_id, addr, conn_state, pair_state, disc_reason);
    } 
    // If we are already connected to a parent, check if this event is about the parent disconnecting.
    else if (g_parent_node.is_valid && memcmp(addr->addr, g_parent_node.addr.addr, SLE_ADDR_LEN) == 0) {
        sle_client_connect_state_changed_cbk(conn_id, addr, conn_state, pair_state, disc_reason);
    } 
    // Otherwise, it must be a child connecting to us.
    else {
        sle_server_connect_state_changed_cbk(conn_id, addr, conn_state, pair_state, disc_reason);
    }
}

// 配对完成事件的中央分发器
static void pair_complete_cbk(uint16_t conn_id, const sle_addr_t *addr, errcode_t status)
{
    if (g_mesh_node_role == NODE_ROLE_GATEWAY) {
        sle_server_pair_complete_cbk(conn_id, addr, status);
        return;
    }

    if (g_parent_node.is_valid && memcmp(addr->addr, g_parent_node.addr.addr, SLE_ADDR_LEN) == 0) {
        sle_client_pair_complete_cbk(conn_id, addr, status);
    } else {
        sle_server_pair_complete_cbk(conn_id, addr, status);
    }
}

// --- SDK回调注册 ---

static errcode_t sle_announce_seek_register_cbks(void)
{
    sle_announce_seek_callbacks_t cbk = {0};
    cbk.sle_enable_cb = sle_enable_cbk;
    cbk.sle_disable_cb = sle_disable_cbk;
    
    // 广播 (服务端) 回调
    cbk.announce_enable_cb = sle_server_announce_enable_cbk;
    cbk.announce_disable_cb = sle_server_announce_disable_cbk;
    cbk.announce_terminal_cb = sle_server_announce_terminal_cbk;

    // 扫描 (客户端) 回调
    cbk.seek_result_cb = sle_client_seek_result_cbk;
    cbk.seek_disable_cb = sle_client_seek_disable_cbk;
    
    return sle_announce_seek_register_callbacks(&cbk);
}

static errcode_t sle_conn_register_cbks(void)
{
    sle_connection_callbacks_t cbk = {0};
    cbk.connect_state_changed_cb = connect_state_changed_cbk;
    cbk.pair_complete_cb = pair_complete_cbk;
    return sle_connection_register_callbacks(&cbk);
}

errcode_t sle_mesh_register_common_cbks(void)
{
    errcode_t ret;
    ret = sle_announce_seek_register_cbks();
    if (ret != ERRCODE_SUCC) {
        osal_printk("[callback] sle_announce_seek_register_cbks FAIL\r\n");
        return ret;
    }

    ret = sle_conn_register_cbks();
    if (ret != ERRCODE_SUCC) {
        osal_printk("[callback] sle_conn_register_cbks FAIL\r\n");
        return ret;
    }
    return ERRCODE_SUCC;
} 