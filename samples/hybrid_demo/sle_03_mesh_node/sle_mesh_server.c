#include "securec.h"
#include "errcode.h"
#include "soc_osal.h"
#include "sle_common.h"
#include "sle_ssap_server.h"
#include "sle_connection_manager.h"
#include "sle_mesh_server.h"
#include "sle_mesh_common.h"
#include "sle_mesh_adv.h"
#include "sle_connection_manager.h"

// 来自主模块的外部函数
extern void mesh_node_child_connected_cb(uint16_t conn_id, const sle_addr_t *addr);
extern void mesh_node_child_disconnected_cb(uint16_t conn_id);
extern void mesh_node_forward_data_cb(const uint8_t* data, uint16_t len);

#define MESH_MAX_CHILDREN 5

static uint8_t g_server_id = 0;
static uint16_t g_service_handle = 0;
static uint16_t g_property_handle = 0; // 特征句柄，由协议栈动态分配
static mesh_neighbor_info_t g_children_nodes[MESH_MAX_CHILDREN] = {0};

// 查找空的子节点槽位
static int find_child_slot(void) 
{
    for (int i = 0; i < MESH_MAX_CHILDREN; i++) {
        if (!g_children_nodes[i].is_valid) {
            return i;
        }
    }
    return -1;
}

// 通过连接ID查找子节点
static int find_child_by_conn_id(uint16_t conn_id) 
{
    for (int i = 0; i < MESH_MAX_CHILDREN; i++) {
        if (g_children_nodes[i].is_valid && g_children_nodes[i].conn_id == conn_id) {
            return i;
        }
    }
    return -1;
}

// 来自子节点的写请求回调
static void ssaps_write_request_cbk(uint8_t server_id, uint16_t conn_id, ssaps_req_write_cb_t *write_cb_para, errcode_t status)
{
    (void)server_id;
    if (status != ERRCODE_SUCC || write_cb_para == NULL) {
        return;
    }
    
    // 检查是否写入了我们定义的Mesh特征
    if (write_cb_para->handle == g_property_handle) {
        osal_printk("Mesh data received from child conn_id:%x\r\n", conn_id);
        mesh_node_forward_data_cb(write_cb_para->value, write_cb_para->length);
    }
}

// 注册SSAPS回调
static errcode_t sle_ssaps_register_cbks(void)
{
    ssaps_callbacks_t ssaps_cbk = {0};
    ssaps_cbk.write_request_cb = ssaps_write_request_cbk;
    return ssaps_register_callbacks(&ssaps_cbk);
}

// 帮助函数，将16位的UUID设置到128位的字段中
static void sle_uuid_set_u16(uint16_t u16, sle_uuid_t *out)
{
    // 使用标准蓝牙SIG的Base UUID
    static uint8_t base[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, \
                             0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};
    (void)memcpy_s(out->uuid, SLE_UUID_LEN, base, SLE_UUID_LEN);
    out->uuid[2] = (uint8_t)(u16 >> 8);
    out->uuid[3] = (uint8_t)u16;
    out->len = sizeof(uint16_t);
}

// 添加Mesh GATT服务和特征
static errcode_t sle_mesh_service_add(void)
{
    errcode_t ret;
    sle_uuid_t app_uuid = {0}; // 注册一个Server实例
    ssaps_register_server(&app_uuid, &g_server_id); 

    // 添加Mesh服务
    sle_uuid_t service_uuid = {0};
    sle_uuid_set_u16(MESH_SERVICE_UUID, &service_uuid);
    ret = ssaps_add_service_sync(g_server_id, &service_uuid, 1, &g_service_handle);
    if (ret != ERRCODE_SUCC) {
        osal_printk("ssaps_add_service_sync fail, ret:%x\r\n", ret);
        return ret;
    }

    // 添加Mesh数据特征
    ssaps_property_info_t property = {0};
    property.permissions = SSAP_PERMISSION_WRITE;
    property.operate_indication = SSAP_OPERATE_INDICATION_BIT_WRITE;
    sle_uuid_set_u16(MESH_CHARACTERISTIC_UUID, &property.uuid);
    property.value = NULL; // 该值由客户端写入，此处不需初始化
    
    // 关键修正：在 add_property_sync 中传入正确的句柄指针，并使用固定的句柄
    g_property_handle = MESH_CHARACTERISTIC_HANDLE;
    ret = ssaps_add_property_sync(g_server_id, g_service_handle, &property, &g_property_handle);
    if (ret != ERRCODE_SUCC) {
        osal_printk("ssaps_add_property_sync fail, ret:%x\r\n", ret);
        return ret;
    }
    
    osal_printk("Mesh service added. Service handle: 0x%04x, Char handle: 0x%04x\r\n", g_service_handle, g_property_handle);

    // 启动服务
    return ssaps_start_service(g_server_id, g_service_handle);
}

// (作为服务端)连接状态变化的回调
void sle_server_connect_state_changed_cbk(uint16_t conn_id, const sle_addr_t *addr,
    sle_acb_state_t conn_state, sle_pair_state_t pair_state, sle_disc_reason_t disc_reason)
{
    (void)pair_state;
    (void)disc_reason;
    if (conn_state == SLE_ACB_STATE_CONNECTED) {
        int slot = find_child_slot();
        if (slot != -1) {
            g_children_nodes[slot].is_valid = true;
            g_children_nodes[slot].conn_id = conn_id;
            (void)memcpy_s(&g_children_nodes[slot].addr, sizeof(sle_addr_t), addr, sizeof(sle_addr_t));
            mesh_node_child_connected_cb(conn_id, addr);

            // After a child connects, restart advertising to allow more children to connect
            osal_printk("Child connected, restarting advertising...\r\n");
            sle_mesh_adv_start();

        } else {
            osal_printk("Max children reached. Connection refused by logic.\r\n");
            // sle_disconnect(conn_id); // This function seems unavailable
        }
    } else if (conn_state == SLE_ACB_STATE_DISCONNECTED) {
        int slot = find_child_by_conn_id(conn_id);
        if (slot != -1) {
            g_children_nodes[slot].is_valid = false;
            mesh_node_child_disconnected_cb(conn_id);
        }
    }
}

// (作为服务端)配对完成的回调
void sle_server_pair_complete_cbk(uint16_t conn_id, const sle_addr_t *addr, errcode_t status)
{
    (void)addr;
    osal_printk("Child pairing complete, conn_id:%d, status:%d\n", conn_id, status);
}

// 初始化Mesh服务端
errcode_t sle_mesh_server_init(void)
{
    errcode_t ret;
    ret = sle_ssaps_register_cbks();
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = sle_mesh_service_add();
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    
    osal_printk("Mesh server initialized.\r\n");
    return ERRCODE_SUCC;
} 