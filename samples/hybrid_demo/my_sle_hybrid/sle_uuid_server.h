/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd.. 2022. All rights reserved.
 *
 * Description: SLE UUID Server module.
 */

/**
 * @defgroup SLE UUID SERVER API
 * @ingroup
 * @{
 */
#ifndef SLE_UUID_SERVER_H
#define SLE_UUID_SERVER_H

#include "errcode.h"
#include "sle_ssap_server.h"

/* Service UUID */
#define SLE_UUID_SERVER_SERVICE        0xABCD

/* Property UUID */
#define SLE_UUID_SERVER_NTF_REPORT     0x1122

/* Property Property */
#define SLE_UUID_TEST_PROPERTIES  (SSAP_PERMISSION_READ | SSAP_PERMISSION_WRITE)

/* Operation indication */
#define SLE_UUID_HYBRID_OPERATION_INDICATION  (SSAP_OPERATE_INDICATION_BIT_READ | SSAP_OPERATE_INDICATION_BIT_WRITE)

/* Descriptor Property */
#define SLE_UUID_TEST_DESCRIPTOR   (SSAP_PERMISSION_READ | SSAP_PERMISSION_WRITE)



errcode_t sle_hybrids_init(void);
int sle_hybrids_send_data(uint8_t *data,uint8_t length);
uint8_t sle_hybrids_is_client_connected(void);
void sle_hybrids_wait_client_connected(void);
#endif
