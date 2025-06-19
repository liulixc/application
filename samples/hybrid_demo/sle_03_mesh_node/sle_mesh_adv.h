/*
 * Copyright (c) dt-sir. All rights reserved.
 *
 * Description: SLE MESH ADV Config.
 */

#ifndef SLE_MESH_ADV_H
#define SLE_MESH_ADV_H
#include "errcode.h"

/**
 * @brief  初始化Mesh广播参数
 * @return errcode_t 运行结果
 */
errcode_t sle_mesh_adv_init(void);

/**
 * @brief  启动Mesh广播
 * @return errcode_t 运行结果
 */
errcode_t sle_mesh_adv_start(void);

/**
 * @brief  停止Mesh广播
 * @return errcode_t 运行结果
 */
errcode_t sle_mesh_adv_stop(void);

/**
 * @brief  更新Mesh广播内容并重启
 * @return errcode_t 运行结果
 */
errcode_t sle_mesh_adv_update(void);

#endif 