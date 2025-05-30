/**
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2022-2023. All rights reserved.
 *
 * Description: Application core main function for standard \n
 *
 * History: \n
 * 2022-07-27, Create file. \n
 */

#ifndef WIFI_CONNECT_H
#define WIFI_CONNECT_H

#include <stdint.h>
#include <td_type.h>

int wifi_connect(const char *ssid, const char *psk);
int wifi_disconnect(void);
td_s32 example_sta_function(const char *ssid, const char *psk);
int check_wifi_status(void);
#endif