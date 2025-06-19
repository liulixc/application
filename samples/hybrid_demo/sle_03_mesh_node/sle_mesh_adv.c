#include "securec.h"
#include "errcode.h"
#include "osal_addr.h"
#include "osal_debug.h"
#include "sle_common.h"
#include "sle_device_discovery.h"
#include "sle_errcode.h"
#include "sle_mesh_adv.h"
#include "sle_mesh_common.h"
#include "sle_adv_const.h"

// 这些全局变量将在主文件 sle_mesh_node.c 中定义和管理
extern uint16_t g_mesh_node_short_addr;
extern uint8_t g_mesh_node_hop_count;
extern uint8_t g_mesh_node_status;

#define SLE_ADV_HANDLE_DEFAULT 1
#define SLE_ADV_INTERVAL_MIN_DEFAULT 0xC8
#define SLE_ADV_INTERVAL_MAX_DEFAULT 0xC8
#define SLE_CONN_INTV_MIN_DEFAULT 0x64
#define SLE_CONN_INTV_MAX_DEFAULT 0x64
#define SLE_CONN_SUPERVISION_TIMEOUT_DEFAULT 0x1F4
#define SLE_CONN_MAX_LATENCY 0x1F3
#define SLE_ADV_DATA_LEN_MAX 251

// 设置Mesh的自定义广播数据
static uint16_t sle_set_mesh_adv_data(uint8_t *adv_data, uint16_t max_len)
{
    if (max_len < 2 + sizeof(mesh_adv_data_t)) {
        return 0;
    }

    // AD Structure: Len + Type + Data
    adv_data[0] = 1 + sizeof(mesh_adv_data_t); // AD Length
    adv_data[1] = MESH_ADV_DATA_TYPE_MANUFACTURER;   // AD Type
    
    mesh_adv_data_t *mesh_data = (mesh_adv_data_t*)&adv_data[2];
    mesh_data->id = g_mesh_node_short_addr;
    mesh_data->hop = g_mesh_node_hop_count;
    mesh_data->status = g_mesh_node_status;
    
    return 2 + sizeof(mesh_adv_data_t);
}

// 设置扫描响应数据（这里我们设置为设备名）
static uint16_t sle_set_scan_response_data(uint8_t *scan_rsp_data, uint16_t max_len)
{
    uint8_t local_name[] = "mesh_node";
    uint8_t local_name_len = (uint8_t)strlen((char *)local_name);
    
    if (max_len < local_name_len + 2) {
        return 0;
    }
    scan_rsp_data[0] = local_name_len + 1;
    scan_rsp_data[1] = SLE_ADV_DATA_TYPE_COMPLETE_LOCAL_NAME;

    if (memcpy_s(&scan_rsp_data[2], max_len - 2, local_name, local_name_len) != EOK) {
        osal_printk("scan response memcpy fail\r\n");
        return 0;
    }
    return (uint16_t)2 + local_name_len;
}

// 配置广播参数
static int sle_set_announce_param_config(void)
{
    sle_announce_param_t param = {0};
    param.announce_mode = SLE_ANNOUNCE_MODE_CONNECTABLE_SCANABLE;
    param.announce_handle = SLE_ADV_HANDLE_DEFAULT;
    param.announce_gt_role = SLE_ANNOUNCE_ROLE_T_CAN_NEGO;
    param.announce_level = SLE_ANNOUNCE_LEVEL_NORMAL;
    param.announce_channel_map = SLE_ADV_CHANNEL_MAP_DEFAULT;
    param.announce_interval_min = SLE_ADV_INTERVAL_MIN_DEFAULT;
    param.announce_interval_max = SLE_ADV_INTERVAL_MAX_DEFAULT;
    param.conn_interval_min = SLE_CONN_INTV_MIN_DEFAULT;
    param.conn_interval_max = SLE_CONN_INTV_MAX_DEFAULT;
    param.conn_max_latency = SLE_CONN_MAX_LATENCY;
    param.conn_supervision_timeout = SLE_CONN_SUPERVISION_TIMEOUT_DEFAULT;
    
    return sle_set_announce_param(param.announce_handle, &param);
}

// 配置广播和扫描响应数据
static int sle_set_announce_data_config(void)
{
    errcode_t ret;
    sle_announce_data_t data = {0};
    static uint8_t announce_data[SLE_ADV_DATA_LEN_MAX] = {0};
    static uint8_t seek_rsp_data[SLE_ADV_DATA_LEN_MAX] = {0};

    (void)memset_s(announce_data, sizeof(announce_data), 0, sizeof(announce_data));
    (void)memset_s(seek_rsp_data, sizeof(seek_rsp_data), 0, sizeof(seek_rsp_data));

    data.announce_data_len = sle_set_mesh_adv_data(announce_data, sizeof(announce_data));
    data.announce_data = announce_data;

    data.seek_rsp_data_len = sle_set_scan_response_data(seek_rsp_data, sizeof(seek_rsp_data));
    data.seek_rsp_data = seek_rsp_data;

    ret = sle_set_announce_data(SLE_ADV_HANDLE_DEFAULT, &data);
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("sle_set_announce_data fail, ret:%x\r\n", ret);
    }
    return ret;
}

// 初始化Mesh广播
errcode_t sle_mesh_adv_init(void)
{
    osal_printk("sle_mesh_adv_init in\r\n");
    sle_set_announce_param_config();
    sle_set_announce_data_config();
    osal_printk("sle_mesh_adv_init out\r\n");
    return ERRCODE_SLE_SUCCESS;
}

// 启动Mesh广播
errcode_t sle_mesh_adv_start(void)
{
    return sle_start_announce(SLE_ADV_HANDLE_DEFAULT);
}

// 停止Mesh广播
errcode_t sle_mesh_adv_stop(void)
{
    return sle_stop_announce(SLE_ADV_HANDLE_DEFAULT);
}

// 更新Mesh广播内容并重启
errcode_t sle_mesh_adv_update(void)
{
    osal_printk("updating mesh adv data...\r\n");
    // 重新配置广播数据，新的跳数和地址会从全局变量中获取
    sle_set_announce_data_config();
    return ERRCODE_SLE_SUCCESS;
}

// --- 广播事件回调实现 ---

void sle_server_announce_enable_cbk(uint32_t announce_id, errcode_t status)
{
    osal_printk("sle announce enable id:%u, status:%d\r\n", announce_id, status);
}

void sle_server_announce_disable_cbk(uint32_t announce_id, errcode_t status)
{
    osal_printk("sle announce disable id:%u, status:%d\r\n", announce_id, status);
}

void sle_server_announce_terminal_cbk(uint32_t announce_id)
{
    osal_printk("sle announce terminal id:%u\r\n", announce_id);
} 