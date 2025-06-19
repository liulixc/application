/*
 * Copyright (c) 2023, HiHope Co., Ltd.
 *
 * Description: SLE Mesh Client Logic
 */
#include <securec.h>
#include "osal_timer.h"
#include "sle_common.h"
#include "sle_mesh_common.h"
#include "sle_mesh_client.h"
#include "errcode.h"
#include "soc_osal.h"
#include "sle_device_discovery.h"
#include "sle_connection_manager.h"
#include "sle_adv_const.h" // For SLE_SEEK constants
#include "sle_ssap_client.h"

// Forward declaration for the node's state setter to avoid circular includes
extern void mesh_node_set_state(mesh_node_state_t new_state);
extern mesh_neighbor_info_t g_parent_node; // For getting parent conn_id
extern void mesh_node_parent_connected_cb(uint16_t conn_id, const sle_addr_t *addr, uint16_t short_addr, uint8_t hop_count);
extern void mesh_node_parent_disconnected_cb(uint16_t conn_id);


#define SLE_MESH_SCAN_TIMEOUT 2000 // ms
#define MESH_CHARACTERISTIC_HANDLE 0x0002

// Module-level globals for tracking the best parent candidate
static uint8_t g_best_parent_hop = 0xFF;
static uint16_t g_best_parent_id = 0;
static sle_addr_t g_best_parent_addr = {0};

static void sle_client_scan_timeout_cb(unsigned long arg);

static osal_timer g_scan_timer;

static bool parse_mesh_adv_data(const uint8_t *data, uint8_t data_len, mesh_adv_data_t *adv_data)
{
    uint8_t pos = 0;
    uint8_t len;
    uint8_t type;

    while (pos < data_len) {
        len = data[pos];
        if (len == 0 || pos + 1 >= data_len) {
            break;
        }
        type = data[pos + 1];

        if (type == MESH_ADV_DATA_TYPE_MANUFACTURER && len >= 1 + sizeof(mesh_adv_data_t)) {
            (void)memcpy_s(adv_data, sizeof(mesh_adv_data_t), &data[pos + 2], sizeof(mesh_adv_data_t));
            return true;
        }
        
        if (pos + len + 1 > pos) {
             pos += len + 1;
        } else {
             break;
        }
    }
    return false;
}


void sle_client_seek_result_cbk(sle_seek_result_info_t *seek_result)
{
    mesh_adv_data_t adv_data = {0};

    if (parse_mesh_adv_data(seek_result->data, seek_result->data_length, &adv_data)) {
        // Rule 1: Only consider nodes that are already connected as potential parents.
        if (adv_data.status != 1) {
            return;
        }

        // Rule 2: Find a parent with a lower hop count.
        if (adv_data.hop < g_best_parent_hop) {
            osal_printk("[seek] Found better parent! hop: %u, id: 0x%04x\r\n", adv_data.hop, adv_data.id);
            g_best_parent_hop = adv_data.hop;
            g_best_parent_id = adv_data.id;
            (void)memcpy_s(&g_best_parent_addr, sizeof(sle_addr_t), &seek_result->addr, sizeof(sle_addr_t));
        }
    }
}

static void sle_client_scan_timeout_cb(unsigned long arg)
{
    (void)arg;
    osal_printk("[timer] Scan timeout, stopping seek.\r\n");
    sle_stop_seek();
}

void sle_client_seek_disable_cbk(errcode_t status)
{
    (void)status;
    osal_timer_stop(&g_scan_timer);
    osal_printk("[cbk] Seek disabled. Parent ID: 0x%04x\r\n", g_best_parent_id);
    if (g_best_parent_id != 0) {
        osal_printk("--> Connecting to parent.\r\n");
        mesh_node_set_state(MESH_NODE_STATE_CONNECTING);
        sle_connect_remote_device(&g_best_parent_addr);
    } else {
        osal_printk("--> No parent found, rescan.\r\n");
        mesh_node_set_state(MESH_NODE_STATE_SCAN);
    }
}


void start_scan(void)
{
    sle_seek_param_t param = {0};

    g_best_parent_hop = 0xFF;
    g_best_parent_id = 0;
    (void)memset_s(&g_best_parent_addr, sizeof(sle_addr_t), 0, sizeof(sle_addr_t));

    param.own_addr_type = SLE_ADDRESS_TYPE_PUBLIC;
    param.seek_phys = SLE_SEEK_PHY_1M;
    param.seek_type[0] = SLE_SEEK_PASSIVE;
    param.seek_interval[0] = 0xA0; // 100ms
    param.seek_window[0] = 0x50;  // 50ms
    param.filter_duplicates = 0; 

    errcode_t ret = sle_set_seek_param(&param);
    if (ret != ERRCODE_SUCC) {
        osal_printk("Set seek param failed, ret:0x%x\r\n", ret);
        mesh_node_set_state(MESH_NODE_STATE_SCAN);
        return;
    }

    ret = sle_start_seek();
    if (ret == ERRCODE_SUCC) {
        osal_printk("Scan started successfully.\r\n");
        osal_timer_start(&g_scan_timer);
    } else {
        osal_printk("Scan start failed, ret:0x%x\r\n", ret);
        mesh_node_set_state(MESH_NODE_STATE_SCAN);
    }
}

void stop_scan(void)
{
    osal_timer_stop(&g_scan_timer);
    sle_stop_seek();
}

errcode_t sle_mesh_client_init(void)
{
    g_scan_timer.handler = sle_client_scan_timeout_cb;
    g_scan_timer.interval = SLE_MESH_SCAN_TIMEOUT;
    
    if (osal_timer_init(&g_scan_timer) != OSAL_SUCCESS) {
        osal_printk("Create scan timer failed!\r\n");
        return ERRCODE_FAIL;
    }
    return ERRCODE_SUCC;
}

void sle_client_connect_state_changed_cbk(uint16_t conn_id, const sle_addr_t *addr,
    sle_acb_state_t conn_state, sle_pair_state_t pair_state, sle_disc_reason_t disc_reason)
{
    (void)pair_state;
    if (conn_state == SLE_ACB_STATE_CONNECTED) {
        osal_printk("Parent connection established. conn_id:%u\r\n", conn_id);
        // We need to find the parent's info (hop, id) that we stored during scan
        // In this simplified logic, we assume the connection is with g_best_parent
        mesh_node_parent_connected_cb(conn_id, addr, g_best_parent_id, g_best_parent_hop);

    } else if (conn_state == SLE_ACB_STATE_DISCONNECTED) {
        osal_printk("Parent connection lost. conn_id:%u, reason:%d\r\n", conn_id, disc_reason);
        mesh_node_parent_disconnected_cb(conn_id);
    }
}

void sle_client_pair_complete_cbk(uint16_t conn_id, const sle_addr_t *addr, errcode_t status)
{
    (void)conn_id;
    (void)addr;
    (void)status;
    osal_printk("Pairing with parent complete, status:%d\r\n", status);
}

// These functions are part of the public API but their logic is handled elsewhere now
errcode_t sle_mesh_client_send_data(const uint8_t *data, uint16_t len)
{
    if (!g_parent_node.is_valid) {
        osal_printk("Fail to send data, parent not connected.\r\n");
        return ERRCODE_FAIL;
    }
    ssapc_write_param_t param = {0};
    param.handle = MESH_CHARACTERISTIC_HANDLE;
    param.type = SSAP_PROPERTY_TYPE_VALUE;
    param.data_len = len;
    param.data = (uint8_t*)data;
    
    return ssapc_write_req(0, g_parent_node.conn_id, &param);
}

void sle_mesh_client_parent_disconnected(void)
{
    // State transition handled in sle_mesh_node.c
}

errcode_t sle_mesh_client_connect(const sle_addr_t *addr)
{
    return sle_connect_remote_device(addr);
} 