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

typedef struct
{
    uint16_t conn_id;                                
    ssapc_find_service_result_t find_service_result; 
} sle_conn_and_service_t;


void sle_set_server_name(char *name);
void sle_hybridc_init(void);
int sle_hybridc_send_data(uint8_t *data, uint8_t length);
void sle_set_hybridc_addr(void);
sle_addr_t *sle_get_remote_server_addr(void);
uint8_t sle_get_num_remote_server_addr(void);
sle_conn_and_service_t *sle_get_conn_and_service(void);
uint8_t sle_hybridc_is_service_found(uint16_t conn_id);

#endif