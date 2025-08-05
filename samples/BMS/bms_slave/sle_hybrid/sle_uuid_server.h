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
 
 /**
  * @brief 节点角色定义
  */
 typedef enum {
     NODE_ROLE_ORPHAN = 0, // 孤儿节点，等待被收养
     NODE_ROLE_MEMBER = 1, // 成员节点，已入网
     NODE_ROLE_GATEWAY = 2, // 网关节点
 } node_role_t;
 
 /**
  * @brief 自定义厂商广播数据结构
  * @note 使用packed确保结构体成员在内存中是紧密排列的，没有填充字节。
  */
 typedef struct {
     uint8_t len;         // 数据长度
     uint8_t type;        // 类型 (0xFF 表示厂商自定义数据)
     uint16_t company_id; // 厂商ID
     uint8_t role;        // 节点角色 (node_role_t)
     uint8_t level;       // 节点在网络中的层级
 } __attribute__((packed)) network_adv_data_t;


 /**
 * @brief 数据上报结构体
 */
typedef struct{
        uint8_t* value;
        uint16_t value_len;
}msg_data_t;

 
 /**
  * @brief "收养"命令结构体
  */
 #define ADOPTION_CMD 0x01 // 定义收养命令的操作码
 typedef struct {
     uint8_t cmd;   // 命令码 (ADOPTION_CMD)
     uint8_t level; // 父节点的层级
 } __attribute__((packed)) adoption_cmd_t;
 
 errcode_t sle_hybrids_init(void);
 int sle_hybrids_send_data(uint8_t *data,uint16_t length);
 uint8_t sle_hybrids_is_client_connected(void);
 void sle_hybrids_wait_client_connected(void);
 #endif