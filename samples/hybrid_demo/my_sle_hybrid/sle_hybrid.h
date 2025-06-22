#ifndef SLE_HYBRID_H
#define SLE_HYBRID_H

#include "sle_uuid_server.h" // 包含以获取 node_role_t 定义
#include "sle_common.h"      // 包含以获取 sle_addr_t 定义

// 函数声明
sle_addr_t* hybrid_get_local_addr(void);
node_role_t hybrid_node_get_role(void);
uint8_t hybrid_node_get_level(void);
uint16_t hybrid_node_get_parent_conn_id(void);
void hybrid_node_become_member(uint16_t parent_conn_id, uint8_t parent_level);
void hybrid_node_revert_to_orphan(void);
bool hybrid_node_is_reverting_to_orphan(void);


#endif 