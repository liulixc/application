/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd.. 2023. All rights reserved.
 * Description: sle adv config for sle uuid server.
 */
#include "securec.h"
#include "errcode.h"
#include "osal_addr.h"
#include "osal_debug.h"
#include "sle_common.h"
#include "sle_device_discovery.h"
#include "sle_errcode.h"
#include "sle_server_adv.h"
#include "sle_uuid_server.h"
#include "sle_hybrid.h"

    
/* sle device name */
#define NAME_MAX_LENGTH 16
/* 连接调度间隔12.5ms，单位125us */
#define SLE_CONN_INTV_MIN_DEFAULT                 0x64
/* 连接调度间隔12.5ms，单位125us */
#define SLE_CONN_INTV_MAX_DEFAULT                 0x64
/* 连接调度间隔25ms，单位125us */
#define SLE_ADV_INTERVAL_MIN_DEFAULT              0xC8
/* 连接调度间隔25ms，单位125us */
#define SLE_ADV_INTERVAL_MAX_DEFAULT              0xC8
/* 超时时间5000ms，单位10ms */
#define SLE_CONN_SUPERVISION_TIMEOUT_DEFAULT      0x1F4
/* 超时时间4990ms，单位10ms */
#define SLE_CONN_MAX_LATENCY                      0x1F3
/* 广播发送功率 */
#define SLE_ADV_TX_POWER  10
/* 最大广播数据长度 */
#define SLE_ADV_DATA_LEN_MAX                      251
/* 广播名称 */
static uint8_t sle_local_name[NAME_MAX_LENGTH] = "hi_mesh_node";


static uint16_t sle_set_adv_local_name(uint8_t *adv_data, uint16_t max_len)
{
    errno_t ret;
    uint8_t index = 0;

    uint8_t *local_name = sle_local_name;
    uint8_t local_name_len = (uint8_t)strlen((char *)local_name);
    for (uint8_t i = 0; i < local_name_len; i++) {
        osal_printk("local_name[%d] = 0x%02x\r\n", i, local_name[i]);
    }

    adv_data[index++] = local_name_len + 1;
    adv_data[index++] = SLE_ADV_DATA_TYPE_COMPLETE_LOCAL_NAME;
    ret = memcpy_s(&adv_data[index], max_len - index, local_name, local_name_len);
    if (ret != EOK) {
        osal_printk("[sle_set_adv_local_name] memcpy fail\r\n");
        return 0;
    }
    uint16_t name_len_total = (uint16_t)index + local_name_len;

    /* 添加自定义网络状态广播 */
    network_adv_data_t net_adv_data = {
        .len = sizeof(network_adv_data_t) - 1, // len字段不包含自身
        .type = 0xFF, // Manufacturer Specific Data
        .company_id = 0x0040, // 假设的公司ID
        .role = hybrid_node_get_role(),
        .level = hybrid_node_get_level(),
    };
    if (max_len - name_len_total < sizeof(net_adv_data)) {
        osal_printk("[sle_set_adv_local_name] not enough space for net adv data\r\n");
        return name_len_total;
    }
    ret = memcpy_s(&adv_data[name_len_total], max_len - name_len_total, &net_adv_data, sizeof(net_adv_data));
    if (ret != EOK) {
        osal_printk("[sle_set_adv_local_name] net adv data memcpy fail\r\n");
        return name_len_total;
    }

    return name_len_total + (uint16_t)sizeof(net_adv_data);
}

static uint16_t sle_set_adv_data(uint8_t *adv_data)
{
    uint16_t idx = 0;
    errno_t ret;

    /* 添加自定义网络状态广播 */
    network_adv_data_t net_adv_data = {
        .len = sizeof(network_adv_data_t) - 1, // len字段不包含自身
        .type = 0xFF, // Manufacturer Specific Data
        .company_id = 0x0040, // 厂商ID (示例)
        .role = hybrid_node_get_role(),
        .level = hybrid_node_get_level(),
    };

    ret = memcpy_s(&adv_data[idx], SLE_ADV_DATA_LEN_MAX - idx, &net_adv_data, sizeof(net_adv_data));
    if (ret != EOK) {
        osal_printk("net_adv_data memcpy fail\r\n");
        return 0;
    }
    idx += sizeof(net_adv_data);
    return idx;
}


static uint16_t sle_set_scan_response_data(uint8_t *scan_rsp_data)
{
    uint16_t idx = 0;
    
    /* 在扫描响应中设置设备名称，方便调试 */
    idx += sle_set_adv_local_name(&scan_rsp_data[idx], SLE_ADV_DATA_LEN_MAX - idx);
    return idx;
}

static int sle_set_default_announce_param(void)
{
    errcode_t ret = 0;
    sle_announce_param_t param = {0};
    uint8_t index;
    sle_addr_t *local_addr = hybrid_get_local_addr();

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
    ret = memcpy_s(param.own_addr.addr, SLE_ADDR_LEN, local_addr->addr, SLE_ADDR_LEN);
    if (ret != EOK) {
        osal_printk("[server] sle_set_default_announce_param data memcpy fail\r\n");
        return 0;
    }
    return sle_set_announce_param(param.announce_handle, &param);
}

static int sle_set_default_announce_data(void)
{
    errcode_t ret;
    uint8_t announce_data_len = 0;
    uint8_t seek_data_len = 0;
    sle_announce_data_t data = {0};
    uint8_t adv_handle = SLE_ADV_HANDLE_DEFAULT;
    uint8_t announce_data[SLE_ADV_DATA_LEN_MAX] = {0};
    uint8_t seek_rsp_data[SLE_ADV_DATA_LEN_MAX] = {0};

    osal_printk("set adv data default\r\n");
    announce_data_len = sle_set_adv_data(announce_data);
    data.announce_data = announce_data;
    data.announce_data_len = announce_data_len;

    seek_data_len = sle_set_scan_response_data(seek_rsp_data);
    data.seek_rsp_data = seek_rsp_data;
    data.seek_rsp_data_len = seek_data_len;

    ret = sle_set_announce_data(adv_handle, &data);
    if (ret == ERRCODE_SLE_SUCCESS) {
        osal_printk("[SLE DD SDK] set announce data success.");
    } else {
        osal_printk("[SLE DD SDK] set adv param fail.");
    }
    return ERRCODE_SLE_SUCCESS;
}



errcode_t sle_uuid_server_adv_init(void)
{
    osal_printk("sle_uuid_server_adv_init in\r\n");
    sle_set_default_announce_param();
    sle_set_default_announce_data();
    sle_start_announce(SLE_ADV_HANDLE_DEFAULT);
    osal_printk("sle_uuid_server_adv_init out\r\n");
    return ERRCODE_SLE_SUCCESS;
}
