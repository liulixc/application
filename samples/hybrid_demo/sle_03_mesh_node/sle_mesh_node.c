#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "app_init.h"
#include "cmsis_os2.h"
#include "soc_osal.h"
#include "errcode.h"
#include "sle_common.h"
#include "sle_device_discovery.h"
#include "securec.h"

#include "sle_mesh_common.h"
#include "sle_mesh_client.h"
#include "sle_mesh_server.h"
#include "sle_mesh_adv.h"

extern errcode_t sle_mesh_register_common_cbks(void);

// --- 节点配置与状态 ---

// 1. 在这里定义您所有设备的MAC地址
// 注意: 第0个地址将作为网关地址
static const uint8_t CUSTOM_MAC_ADDRESSES[][SLE_ADDR_LEN] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x01}, // 节点ID 0 (网关)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x03}, // 节点ID 1
};

// 2. 每次烧录前，修改这个ID来为开发板分配身份
#define CUSTOM_NODE_ID 1

// 节点全局状态
volatile mesh_node_state_t g_current_state = MESH_NODE_STATE_INIT;
mesh_node_role_t g_mesh_node_role;
uint16_t g_mesh_node_short_addr = MESH_INVALID_NODE_ID;
uint8_t g_mesh_node_hop_count = MESH_INVALID_HOP_COUNT;
uint8_t g_mesh_node_status = 0; // 0: seeking, 1: connected
mesh_neighbor_info_t g_parent_node = {0};

void mesh_node_set_state(mesh_node_state_t new_state)
{
    if (g_current_state != new_state) {
        osal_printk("Node state changed: %d -> %d\r\n", g_current_state, new_state);
        g_current_state = new_state;
    }
}

// --- 数据处理与转发 ---

// 将数据向树的根节点（网关）发送
static void send_data_up(const uint8_t* payload, uint16_t payload_len, uint16_t originator, uint16_t destination)
{
    static mesh_packet_t packet;
    uint16_t packet_len = (uint16_t)(offsetof(mesh_packet_t, payload) + payload_len);

    (void)memset_s(&packet, sizeof(mesh_packet_t), 0, sizeof(mesh_packet_t));
    packet.type = PACKET_TYPE_DATA;
    packet.origin_id = originator;
    packet.dest_id = destination;
    (void)memcpy_s(packet.payload, MESH_MAX_PAYLOAD_SIZE, payload, payload_len);

    if (g_mesh_node_role == NODE_ROLE_GATEWAY) {
        // 网关是数据终点，打印收到的数据
        osal_printk("== GATEWAY RX: from [0x%04x], data: %s ==\r\n", 
                    packet.origin_id, (char*)packet.payload);
    } else {
        // 普通节点将数据发送给父节点
        if (g_parent_node.is_valid) {
            osal_printk("Node [0x%04x]: sending data up.\r\n", g_mesh_node_short_addr);
            sle_mesh_client_send_data((uint8_t*)&packet, packet_len);
        } else {
            osal_printk("Node [0x%04x]: cannot send data, no parent.\r\n", g_mesh_node_short_addr);
        }
    }
}

// 从子节点收到数据，需要转发
void mesh_node_forward_data_cb(const uint8_t* data, uint16_t len)
{
    mesh_packet_t* packet = (mesh_packet_t*)data;
    osal_printk("Node [0x%04x]: forwarding data from [0x%04x] for [0x%04x].\r\n", 
        g_mesh_node_short_addr, packet->origin_id, packet->dest_id);
    
    // 计算载荷长度
    uint16_t payload_len = len - (uint16_t)offsetof(mesh_packet_t, payload);

    // 直接将收到的数据包继续向上传递
    send_data_up(packet->payload, payload_len, packet->origin_id, packet->dest_id);
}

// --- 状态机回调 (由各模块调用) ---

void mesh_node_parent_connected_cb(uint16_t conn_id, const sle_addr_t *addr, uint16_t short_addr, uint8_t hop_count)
{
    osal_printk("Parent connected! Parent addr=0x%04x, hop=%d\r\n", short_addr, hop_count);
    mesh_node_set_state(MESH_NODE_STATE_CONNECTED);

    g_parent_node.is_valid = true;
    g_parent_node.conn_id = conn_id;
    (void)memcpy_s(&g_parent_node.addr, sizeof(sle_addr_t), addr, sizeof(sle_addr_t));
    g_parent_node.short_addr = short_addr;
    g_parent_node.hop_count = hop_count;

    g_mesh_node_hop_count = hop_count + 1;
    g_mesh_node_status = 1; // Set status to connected
    osal_printk("My new hop count is %d. Updating and starting ADV.\r\n", g_mesh_node_hop_count);
    sle_mesh_adv_update();
    sle_mesh_adv_start();
}

void mesh_node_parent_disconnected_cb(uint16_t conn_id)
{
    (void)conn_id;
    osal_printk("Parent disconnected.\r\n");
    g_parent_node.is_valid = false;
    g_mesh_node_hop_count = MESH_INVALID_HOP_COUNT;
    g_mesh_node_status = 0; // Set status to seeking
    sle_mesh_adv_update();
    mesh_node_set_state(MESH_NODE_STATE_SCAN);
}

void mesh_node_child_connected_cb(uint16_t conn_id, const sle_addr_t *addr)
{
    (void)addr;
    osal_printk("Child connected, conn_id=0x%x\r\n", conn_id);
}

void mesh_node_child_disconnected_cb(uint16_t conn_id)
{
    osal_printk("Child disconnected, conn_id=0x%x\r\n", conn_id);
}

// --- SLE协议栈使能/失能回调 ---

void sle_mesh_node_enable_cbk(errcode_t status)
{
    if (status != ERRCODE_SUCC) {
        osal_printk("Enable SLE failed, status:%d\r\n", status);
        return;
    }
    osal_printk("Enable SLE success.\r\n");

    // Initialize all modules after SLE stack is enabled
    sle_mesh_client_init();
    sle_mesh_server_init();
    sle_mesh_adv_init();

    // After sle is enabled, start the state machine
    if (g_mesh_node_role == NODE_ROLE_GATEWAY) {
        mesh_node_set_state(MESH_NODE_STATE_IDLE); // Gateway will just advertise
        sle_mesh_adv_start();
    } else {
        mesh_node_set_state(MESH_NODE_STATE_SCAN);
    }
}

void sle_mesh_node_disable_cbk(errcode_t status)
{
    (void)status;
    osal_printk("SLE disabled.\r\n");
}

// --- 主任务 ---

void sle_mesh_node_task(void *arg)
{
    (void)arg;
    sle_addr_t custom_addr = {0};

    // 1. 提前根据ID确定角色和地址
    if (CUSTOM_NODE_ID < (sizeof(CUSTOM_MAC_ADDRESSES) / SLE_ADDR_LEN)) {
        (void)memcpy_s(custom_addr.addr, SLE_ADDR_LEN, CUSTOM_MAC_ADDRESSES[CUSTOM_NODE_ID], SLE_ADDR_LEN);
    } else {
        osal_printk("Error: CUSTOM_NODE_ID is out of bounds!\r\n");
        return;
    }
    
    g_mesh_node_short_addr = (uint16_t)((custom_addr.addr[4] << 8) | custom_addr.addr[5]);
    if (CUSTOM_NODE_ID == 0) {
        g_mesh_node_role = NODE_ROLE_GATEWAY;
        g_mesh_node_hop_count = 0;
        g_mesh_node_status = 1; // Gateway is always connected
        osal_printk("Role: GATEWAY. Address: 0x%04x\r\n", g_mesh_node_short_addr);
    } else {
        g_mesh_node_role = NODE_ROLE_NODE;
        g_mesh_node_hop_count = MESH_INVALID_HOP_COUNT;
        g_mesh_node_status = 0; // Nodes start by seeking
        osal_printk("Role: NODE. Address: 0x%04x\r\n", g_mesh_node_short_addr);
    }

    // 2. 注册所有回调
    sle_mesh_register_common_cbks();

    // 3. 使能SLE协议栈, 这会触发 sle_mesh_node_enable_cbk
    enable_sle();
    
    // 4. 在使能SLE之后，再调用sle_set_local_addr设置自定义MAC地址
    custom_addr.type = SLE_ADDRESS_TYPE_PUBLIC;
    sle_set_local_addr(&custom_addr);
    osal_printk("Set custom MAC after enable: %02x:%02x:%02x:%02x:%02x:%02x\r\n",
                custom_addr.addr[0], custom_addr.addr[1], custom_addr.addr[2],
                custom_addr.addr[3], custom_addr.addr[4], custom_addr.addr[5]);

    // 5. 主循环 - 状态机
    uint32_t message_count = 0;
    while (1) {
        switch (g_current_state) {
            case MESH_NODE_STATE_SCAN:
                mesh_node_set_state(MESH_NODE_STATE_IDLE);
                start_scan();
                break;
            
            case MESH_NODE_STATE_CONNECTED:
                if (g_mesh_node_role == NODE_ROLE_NODE && g_parent_node.is_valid) {
                    char data_payload[40];
                    int ret = sprintf_s(data_payload, sizeof(data_payload), "v:3.7,c:1.2,n:%u", message_count++);
                    if (ret > 0) {
                        send_data_up((uint8_t*)data_payload, (uint16_t)ret, g_mesh_node_short_addr, MESH_GATEWAY_ID);
                    }
                }
                break;

            case MESH_NODE_STATE_INIT:
            case MESH_NODE_STATE_CONNECTING:
            case MESH_NODE_STATE_IDLE:
            default:
                break;
        }
        osDelay(2000);
    }
}

#define SLE_MESH_TASK_PRIO 24
#define SLE_MESH_STACK_SIZE 0x2000

static void sle_mesh_node_entry(void)
{
    osal_task *task_handle = NULL;
    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)sle_mesh_node_task, NULL, "sle_mesh_node", SLE_MESH_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, SLE_MESH_TASK_PRIO);
        osal_kfree(task_handle);
    }
    osal_kthread_unlock();
}

app_run(sle_mesh_node_entry); 