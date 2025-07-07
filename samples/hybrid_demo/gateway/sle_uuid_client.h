/**
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2023-2023. All rights reserved.
 *
 * Description: SLE Mesh Gateway Client.
 *
 */
 #ifndef SLE_GATEWAY_CLIENT_H
 #define SLE_GATEWAY_CLIENT_H
 
 #include "sle_ssap_client.h"
 #include <stdbool.h>
 
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
  */
 typedef struct {
     uint8_t len;         // 数据长度
     uint8_t type;        // 类型 (0xFF 表示厂商自定义数据)
     uint16_t company_id; // 厂商ID
     uint8_t role;        // 节点角色 (node_role_t)
     uint8_t level;       // 节点在网络中的层级
 } __attribute__((packed)) network_adv_data_t;
 
 /**
  * @brief "收养"命令结构体
  */
 #define ADOPTION_CMD 0x01 // 定义收养命令的操作码
 typedef struct {
     uint8_t cmd;   // 命令码 (ADOPTION_CMD)
     uint8_t level; // 父节点的层级
 } __attribute__((packed)) adoption_cmd_t;
 
 /**
  * @brief 数据上报结构体
  */
 typedef struct {
     uint8_t origin_mac[6]; // 原始数据节点的MAC地址
     uint32_t data;          // 示例数据（例如一个计数器）
 } __attribute__((packed)) report_data_t;
 
 // 子节点连接信息管理结构体
 typedef struct {
     uint16_t conn_id;       // 连接ID
     bool is_active;         // 是否活跃
     uint8_t mac[6];         // 子节点的MAC地址
     uint16_t write_handle;  // 为该子节点存储的可写句柄
 } child_node_info_t;
 
 // 设备映射结构体
 typedef struct {
     uint16_t conn_id;           // 设备ID
     uint8_t device_index;       // g_env_msg中的设备索引
     bool is_active;             // 设备是否活跃
     uint8_t device_mac[6];      // 设备MAC地址
     char cloud_device_id[64];   // 设备ID (如 "680b91649314d11851158e8d_Battery05")
 } bms_device_map_t;
 
 // 初始化函数
 void sle_gateway_client_init(void);
 
 // 获取当前连接的子节点数量
 uint8_t get_active_children_count(void);
 
 // 获取活跃设备数量
 uint8_t get_active_device_count(void);
 
 #endif