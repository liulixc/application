/*
 * Copyright (c) dt-sir. All rights reserved.
 *
 * Description: SLE MESH Server module.
 */
#ifndef SLE_MESH_SERVER_H
#define SLE_MESH_SERVER_H

#include "errcode.h"
#include "sle_ssap_server.h"

// 我们为Mesh通信定义的GATT服务
#define MESH_SERVICE_UUID        0xA00A
// 用于接收和转发Mesh数据的特征
#define MESH_CHARACTERISTIC_UUID 0xB00B
// Mesh特征的句柄(handle), 客户端将向此句柄写入数据。
// 这个值必须与客户端(sle_mesh_client.c)中的值匹配。
// GATT规范中，句柄由服务端在添加服务时动态分配。为简化实现，我们在此固定它。
// 注意：这在实际产品中不推荐，但对于原型验证是可行的。
#define MESH_CHARACTERISTIC_HANDLE 0x25 

/**
 * @brief 初始化Mesh服务端模块
 * @return errcode_t 运行结果
 */
errcode_t sle_mesh_server_init(void);

#endif 