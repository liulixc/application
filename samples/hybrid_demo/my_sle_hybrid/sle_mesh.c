/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2024. All rights reserved.
 *
 * Description: SLE Mesh implementation.
 */

#include "sle_mesh.h"
#include "sle_server_adv.h"
#include "securec.h"
#include "soc_osal.h"
#include "sle_device_discovery.h"
#include "sle_connection_manager.h"

typedef struct {
    uint32_t seq;
    sle_addr_t originator_addr;
} mesh_msg_cache_entry_t;

static mesh_msg_cache_entry_t g_msg_cache[MESH_MSG_CACHE_SIZE];
static uint8_t g_msg_cache_idx = 0;
static uint32_t g_sequence_number = 0;
static sle_addr_t g_local_addr;

void sle_mesh_init(void)
{
    (void)memset_s(g_msg_cache, sizeof(g_msg_cache), 0, sizeof(g_msg_cache));
    sle_get_local_addr(&g_local_addr);
}

static bool is_msg_in_cache(uint32_t seq, const sle_addr_t *originator)
{
    for (int i = 0; i < MESH_MSG_CACHE_SIZE; i++) {
        if (g_msg_cache[i].seq == seq && memcmp(&g_msg_cache[i].originator_addr, originator, sizeof(sle_addr_t)) == 0) {
            return true;
        }
    }
    return false;
}

static void add_msg_to_cache(uint32_t seq, const sle_addr_t *originator)
{
    g_msg_cache[g_msg_cache_idx].seq = seq;
    (void)memcpy_s(&g_msg_cache[g_msg_cache_idx].originator_addr, sizeof(sle_addr_t), originator, sizeof(sle_addr_t));
    g_msg_cache_idx = (g_msg_cache_idx + 1) % MESH_MSG_CACHE_SIZE;
}

static errcode_t sle_mesh_forward_packet(sle_mesh_packet_t *packet, uint16_t packet_len)
{
    packet->ttl--;
    if (packet->ttl > 0) {
        osal_printk("Forwarding mesh packet, seq: %u, new ttl: %d\r\n", packet->seq, packet->ttl);
        return sle_server_set_and_update_mesh_adv_data((const uint8_t *)packet, packet_len);
    }
    return ERRCODE_SUCC;
}

errcode_t sle_mesh_send_data(const uint8_t *data, uint8_t length)
{
    if (length > MESH_MAX_PAYLOAD) {
        return ERRCODE_FAIL;
    }
    uint16_t packet_len = sizeof(sle_mesh_packet_t) - MESH_MAX_PAYLOAD + length;
    sle_mesh_packet_t *packet = osal_vmalloc(packet_len);
    if (packet == NULL) {
        return ERRCODE_FAIL;
    }

    packet->ttl = MESH_MAX_HOPS;
    packet->seq = g_sequence_number++;
    (void)memcpy_s(&packet->originator_addr, sizeof(sle_addr_t), &g_local_addr, sizeof(sle_addr_t));
    packet->payload_len = length;
    (void)memcpy_s(packet->payload, length, data, length);
    
    add_msg_to_cache(packet->seq, &packet->originator_addr);
    
    errcode_t ret = sle_server_set_and_update_mesh_adv_data((const uint8_t *)packet, packet_len);

    osal_vfree(packet);
    return ret;
}


void sle_mesh_process_adv_packet(const sle_addr_t *adv_addr, uint8_t *data, uint16_t len)
{
    // The data is Manufacturer Specific Data field from advertisement (including type).
    if (data == NULL || len < 5) { // 1(type) + 2(company_id) + at least 2 for payload
        return;
    }

    if (data[0] != SLE_ADV_DATA_TYPE_MANUFACTURER_SPECIFIC_DATA) {
        return;
    }

    uint16_t company_id = (data[2] << 8) | data[1]; // LSB first
    if (company_id != MESH_COMPANY_ID) {
        return; // Not our mesh packet
    }
    
    sle_mesh_packet_t *packet = (sle_mesh_packet_t*)&data[3];
    uint16_t packet_len = len - 3;
    uint16_t expected_packet_len = sizeof(sle_mesh_packet_t) - MESH_MAX_PAYLOAD + packet->payload_len;
    
    if (packet_len < (sizeof(sle_mesh_packet_t) - MESH_MAX_PAYLOAD) || packet_len != expected_packet_len) {
        osal_printk("Received corrupted mesh packet\r\n");
        return;
    }

    if (memcmp(&packet->originator_addr, &g_local_addr, sizeof(sle_addr_t)) == 0) {
        return; // Received our own packet
    }

    if (is_msg_in_cache(packet->seq, &packet->originator_addr)) {
        return; // Already processed
    }

    add_msg_to_cache(packet->seq, &packet->originator_addr);

    // Process the payload
    osal_printk("Received mesh packet from %02x:%02x:%02x:%02x:%02x:%02x, seq: %u, ttl: %d, payload: %.*s\r\n",
        packet->originator_addr.addr[0], packet->originator_addr.addr[1], packet->originator_addr.addr[2],
        packet->originator_addr.addr[3], packet->originator_addr.addr[4], packet->originator_addr.addr[5],
        packet->seq, packet->ttl, packet->payload_len, packet->payload);
    
    // Forward the packet
    sle_mesh_forward_packet(packet, expected_packet_len);
} 