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
/**
 * @if Eng
 * @brief  sle uuid client init.
 * @attention  NULL
 * @retval ERRCODE_SLE_SUCCESS    Excute successfully
 * @retval ERRCODE_SLE_FAIL       Execute fail
 * @par Dependency:
 * @li NULL
 * @else
 * @brief  sle uuid客户端初始化。
 * @attention  NULL
 * @retval ERRCODE_SLE_SUCCESS    执行成功
 * @retval ERRCODE_SLE_FAIL       执行失败
 * @par 依赖:
 * @li NULL
 * @endif
 */
void sle_client_init(void);

/**
 * @if Eng
 * @brief  sle start scan.
 * @attention  NULL
 * @retval ERRCODE_SLE_SUCCESS    Excute successfully
 * @retval ERRCODE_SLE_FAIL       Execute fail
 * @par Dependency:
 * @li NULL
 * @else
 * @brief  sle启动扫描。
 * @attention  NULL
 * @retval ERRCODE_SLE_SUCCESS    执行成功
 * @retval ERRCODE_SLE_FAIL       执行失败
 * @par 依赖:
 * @li NULL
 * @endif
 */
void sle_start_scan(void);

void sle_set_remote_server_name(char *name);

errcode_t sle_hybridC_init(void);

sle_addr_t *sle_get_remote_server_addr(void);

void sle_set_hybridc_addr(void);

void sle_hybridc_wait_service_found(void);

int sle_hybridc_send_data(uint8_t *data, uint8_t length);
#endif