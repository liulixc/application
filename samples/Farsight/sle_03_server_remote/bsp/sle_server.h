/*
 * Copyright (c) 2024 HiSilicon Technologies CO., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SLE_SERVER_H
#define SLE_SERVER_H

#include <stdint.h>
#include "sle_ssap_server.h"
#include "errcode.h"
#include "common_def.h"
#include "pinctrl.h"
#include "securec.h"
#include "soc_osal.h"
#include "sle_errcode.h"
#include "sle_connection_manager.h"
#include "sle_device_discovery.h"
#include "sle_server_adv.h"
#include "bsp_init.h"
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

extern uint16_t g_sle_conn_flag;

errcode_t sle_server_init(void);

errcode_t sle_server_send_report_by_uuid(msg_data_t msg_data);

errcode_t sle_server_send_report_by_handle(msg_data_t msg_data);

uint16_t sle_client_is_connected(void);

uint16_t get_connect_id(void);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif