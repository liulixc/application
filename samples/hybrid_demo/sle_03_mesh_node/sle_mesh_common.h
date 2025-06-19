#ifndef SLE_MESH_COMMON_H
#define SLE_MESH_COMMON_H

#include <stdbool.h>
#include <stdint.h>
#include "sle_common.h"

#define MESH_MAX_PAYLOAD_SIZE 128
#define MESH_INVALID_HOP_COUNT 0xFF
#define MESH_GATEWAY_ID 0x0001
#define MESH_INVALID_NODE_ID 0xFFFF
#define MESH_ADV_DATA_TYPE_CUSTOM 0xDD // 自定义广播类型
#define MESH_ADV_DATA_TYPE_MANUFACTURER 0xFF

// Mesh数据包类型
typedef enum {
    MESH_MSG_TYPE_DATA_UP,          // 数据包：节点 -> 网关
    MESH_MSG_TYPE_DATA_DOWN,        // 数据包（命令）：网关 -> 节点
} mesh_msg_type_t;

// Mesh网络中广播的数据结构
typedef struct {
    uint16_t id;       // 广播节点的短地址
    uint8_t hop;       // 节点到网关的跳数
    uint8_t status;    // 0: seeking parent, 1: connected
} __attribute__((packed)) mesh_adv_data_t;

// Mesh网络传输的数据包结构
typedef struct {
    uint8_t type;
    uint16_t origin_id;
    uint16_t dest_id;
    uint8_t payload[MESH_MAX_PAYLOAD_SIZE];
} __attribute__((packed)) mesh_packet_t;

// 节点在Mesh网络中的角色
typedef enum {
    NODE_ROLE_GATEWAY,
    NODE_ROLE_NODE,
} mesh_node_role_t;

// 存储父/子节点信息的结构体
typedef struct {
    bool is_valid;
    uint16_t conn_id;
    sle_addr_t addr;
    uint16_t short_addr; // 节点的短地址
    uint8_t hop_count;   // 节点的跳数
} mesh_neighbor_info_t;

typedef enum {
    PACKET_TYPE_DATA,
    PACKET_TYPE_COMMAND,
    PACKET_TYPE_HEARTBEAT,
} mesh_packet_type_t;

typedef enum {
    MESH_NODE_STATE_INIT,
    MESH_NODE_STATE_SCAN,
    MESH_NODE_STATE_CONNECTING,
    MESH_NODE_STATE_CONNECTED,
    MESH_NODE_STATE_IDLE,
} mesh_node_state_t;

#endif // SLE_MESH_COMMON_H 