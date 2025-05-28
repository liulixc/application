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

/**
 * @brief SLE UUID客户端初始化接口
 *
 * @param notification_cb   通知回调函数
 * @param indication_cb     指示回调函数
 * @return 无
 *
 * 用于初始化SLE客户端，注册通知和指示回调。
 */
void sle_client_init(ssapc_notification_callback notification_cb, ssapc_indication_callback indication_cb);

/**
 * @brief 启动SLE扫描
 *
 * @return 无
 *
 * 用于启动SLE设备的扫描流程。
 */
void sle_start_scan(void);

#endif