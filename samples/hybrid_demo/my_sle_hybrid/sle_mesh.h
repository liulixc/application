/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2024. All rights reserved.
 *
 * Description: SLE Mesh definitions.
 */

#ifndef SLE_MESH_H
#define SLE_MESH_H

#include "stdint.h"
#include "errcode.h"
#include "sle_common.h"

#define MESH_MAX_PAYLOAD 64
#define MESH_MAX_HOPS    4
#define MESH_MSG_CACHE_SIZE 32
#define MESH_COMPANY_ID  0xABCD

typedef struct {
    uint8_t ttl;
    uint32_t seq;
    sle_addr_t originator_addr;
    uint8_t payload_len;
    uint8_t payload[MESH_MAX_PAYLOAD];
} __attribute__((packed)) sle_mesh_packet_t;

void sle_mesh_init(void);
errcode_t sle_mesh_send_data(const uint8_t *data, uint8_t length);
void sle_mesh_process_adv_packet(const sle_addr_t *adv_addr, uint8_t *data, uint16_t len);

#endif /* SLE_MESH_H */ 