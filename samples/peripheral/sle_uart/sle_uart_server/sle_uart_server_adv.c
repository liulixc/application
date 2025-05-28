/**
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2023-2023. All rights reserved.
 *
 * Description: sle adv config for sle uart server. \n
 *
 * History: \n
 * 2023-07-17, Create file. \n
 */

// ================== sle_uart_server_adv.c 广播相关实现 ==================
/**
 * 本文件实现了SLE UART Server的广播参数、广播数据、扫描响应数据的配置，
 * 以及广播相关回调的注册和初始化。
 * 主要功能：
 * 1. 设置广播参数 sle_set_default_announce_param
 * 2. 设置广播数据 sle_set_default_announce_data
 * 3. 设置扫描响应数据 sle_set_scan_response_data
 * 4. 注册广播相关回调 sle_uart_announce_register_cbks
 * 5. 广播初始化 sle_uart_server_adv_init
 */

// ================== 1. 头文件与宏定义区 ==================
#include "securec.h"
#include "errcode.h"
#include "osal_addr.h"
#include "product.h"
#include "sle_common.h"
#include "sle_uart_server.h"
#include "sle_device_discovery.h"
#include "sle_errcode.h"
#include "osal_debug.h"
#include "osal_task.h"
#include "string.h"
#include "sle_uart_server_adv.h"

// ================== 2. 函数声明区 ==================
static uint16_t sle_set_adv_local_name(uint8_t *adv_data, uint16_t max_len);
static uint16_t sle_set_adv_data(uint8_t *adv_data);
static uint16_t sle_set_scan_response_data(uint8_t *scan_rsp_data);
static int sle_set_default_announce_param(void);
static int sle_set_default_announce_data(void);
static void sle_announce_enable_cbk(uint32_t announce_id, errcode_t status);
static void sle_announce_disable_cbk(uint32_t announce_id, errcode_t status);
static void sle_announce_terminal_cbk(uint32_t announce_id);
errcode_t sle_uart_announce_register_cbks(void);
errcode_t sle_uart_server_adv_init(void);

// ================== 3. 静态变量定义区 ==================
/* sle device name */
#define NAME_MAX_LENGTH 16 // 设备名最大长度
#define SLE_CONN_INTV_MIN_DEFAULT 0x64 // 连接最小间隔（12.5ms）
#define SLE_CONN_INTV_MAX_DEFAULT 0x64 // 连接最大间隔（12.5ms）
#define SLE_ADV_INTERVAL_MIN_DEFAULT 0xC8 // 广播最小间隔（25ms）
#define SLE_ADV_INTERVAL_MAX_DEFAULT 0xC8 // 广播最大间隔（25ms）
#define SLE_CONN_SUPERVISION_TIMEOUT_DEFAULT 0x1F4 // 连接超时（5000ms）
#define SLE_CONN_MAX_LATENCY 0x1F3 // 最大连接延迟（4990ms）
#define SLE_ADV_TX_POWER  10 // 广播发送功率
#define SLE_ADV_HANDLE_DEFAULT 1 // 广播句柄
#define SLE_ADV_DATA_LEN_MAX 251 // 广播数据最大长度
static uint8_t sle_local_name[NAME_MAX_LENGTH] = "sle_uart_server"; // 广播名称
#define SLE_SERVER_INIT_DELAY_MS    1000 // 初始化延时
#define sample_at_log_print(fmt, args...) osal_printk(fmt, ##args)
#define SLE_UART_SERVER_LOG "[sle uart server]"

// ================== 4. 广播数据与参数设置相关实现 ==================
/**
 * @brief 设置广播数据中的本地名称
 * @param adv_data 广播数据缓冲区
 * @param max_len  缓冲区最大长度
 * @return 实际写入长度
 */
static uint16_t sle_set_adv_local_name(uint8_t *adv_data, uint16_t max_len)
{
    errno_t ret;
    uint8_t index = 0;

    uint8_t *local_name = sle_local_name;
    uint8_t local_name_len = sizeof(sle_local_name) - 1;
    sample_at_log_print("%s local_name_len = %d\r\n", SLE_UART_SERVER_LOG, local_name_len);
    sample_at_log_print("%s local_name: ", SLE_UART_SERVER_LOG);
    for (uint8_t i = 0; i < local_name_len; i++) {
        sample_at_log_print("0x%02x ", local_name[i]);
    }
    sample_at_log_print("\r\n");
    adv_data[index++] = local_name_len + 1;
    adv_data[index++] = SLE_ADV_DATA_TYPE_COMPLETE_LOCAL_NAME;
    ret = memcpy_s(&adv_data[index], max_len - index, local_name, local_name_len);
    if (ret != EOK) {
        sample_at_log_print("%s memcpy fail\r\n", SLE_UART_SERVER_LOG);
        return 0;
    }
    return (uint16_t)index + local_name_len;
}

/**
 * @brief 设置广播数据（发现等级、访问模式）
 * @param adv_data 广播数据缓冲区
 * @return 实际写入长度
 */
static uint16_t sle_set_adv_data(uint8_t *adv_data)
{
    size_t len = 0;
    uint16_t idx = 0;
    errno_t  ret = 0;

    len = sizeof(struct sle_adv_common_value);
    struct sle_adv_common_value adv_disc_level = {
        .length = len - 1,
        .type = SLE_ADV_DATA_TYPE_DISCOVERY_LEVEL,
        .value = SLE_ANNOUNCE_LEVEL_NORMAL,
    };
    ret = memcpy_s(&adv_data[idx], SLE_ADV_DATA_LEN_MAX - idx, &adv_disc_level, len);
    if (ret != EOK) {
        sample_at_log_print("%s adv_disc_level memcpy fail\r\n", SLE_UART_SERVER_LOG);
        return 0;
    }
    idx += len;

    len = sizeof(struct sle_adv_common_value);
    struct sle_adv_common_value adv_access_mode = {
        .length = len - 1,
        .type = SLE_ADV_DATA_TYPE_ACCESS_MODE,
        .value = 0,
    };
    ret = memcpy_s(&adv_data[idx], SLE_ADV_DATA_LEN_MAX - idx, &adv_access_mode, len);
    if (ret != EOK) {
        sample_at_log_print("%s adv_access_mode memcpy fail\r\n", SLE_UART_SERVER_LOG);
        return 0;
    }
    idx += len;

    return idx;
}

/**
 * @brief 设置扫描响应数据（发射功率、本地名称）
 * @param scan_rsp_data 扫描响应数据缓冲区
 * @return 实际写入长度
 */
static uint16_t sle_set_scan_response_data(uint8_t *scan_rsp_data)
{
    uint16_t idx = 0;
    errno_t ret;
    size_t scan_rsp_data_len = sizeof(struct sle_adv_common_value);

    struct sle_adv_common_value tx_power_level = {
        .length = scan_rsp_data_len - 1,
        .type = SLE_ADV_DATA_TYPE_TX_POWER_LEVEL,
        .value = SLE_ADV_TX_POWER,
    };
    ret = memcpy_s(scan_rsp_data, SLE_ADV_DATA_LEN_MAX, &tx_power_level, scan_rsp_data_len);
    if (ret != EOK) {
        sample_at_log_print("%s sle scan response data memcpy fail\r\n", SLE_UART_SERVER_LOG);
        return 0;
    }
    idx += scan_rsp_data_len;

    /* set local name */
    idx += sle_set_adv_local_name(&scan_rsp_data[idx], SLE_ADV_DATA_LEN_MAX - idx);
    return idx;
}

/**
 * @brief 设置默认广播参数
 * @return 0成功，其他失败
 */
static int sle_set_default_announce_param(void)
{
    errno_t ret;
    sle_announce_param_t param = {0};
    uint8_t index;
    unsigned char local_addr[SLE_ADDR_LEN] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 };
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
    param.announce_tx_power = 18;
    param.own_addr.type = 0;
    ret = memcpy_s(param.own_addr.addr, SLE_ADDR_LEN, local_addr, SLE_ADDR_LEN);
    if (ret != EOK) {
        sample_at_log_print("%s sle_set_default_announce_param data memcpy fail\r\n", SLE_UART_SERVER_LOG);
        return 0;
    }
    sample_at_log_print("%s sle_uart_local addr: ", SLE_UART_SERVER_LOG);
    for (index = 0; index < SLE_ADDR_LEN; index++) {
        sample_at_log_print("0x%02x ", param.own_addr.addr[index]);
    }
    sample_at_log_print("\r\n");
    return sle_set_announce_param(param.announce_handle, &param);
}

/**
 * @brief 设置默认广播数据和扫描响应数据
 * @return 0成功，其他失败
 */
static int sle_set_default_announce_data(void)
{
    errcode_t ret;
    uint8_t announce_data_len = 0;
    uint8_t seek_data_len = 0;
    sle_announce_data_t data = {0};
    uint8_t adv_handle = SLE_ADV_HANDLE_DEFAULT;
    uint8_t announce_data[SLE_ADV_DATA_LEN_MAX] = {0};
    uint8_t seek_rsp_data[SLE_ADV_DATA_LEN_MAX] = {0};
    uint8_t data_index = 0;

    announce_data_len = sle_set_adv_data(announce_data);
    data.announce_data = announce_data;
    data.announce_data_len = announce_data_len;

    sample_at_log_print("%s data.announce_data_len = %d\r\n", SLE_UART_SERVER_LOG, data.announce_data_len);
    sample_at_log_print("%s data.announce_data: ", SLE_UART_SERVER_LOG);
    for (data_index = 0; data_index<data.announce_data_len; data_index++) {
        sample_at_log_print("0x%02x ", data.announce_data[data_index]);
    }
    sample_at_log_print("\r\n");

    seek_data_len = sle_set_scan_response_data(seek_rsp_data);
    data.seek_rsp_data = seek_rsp_data;
    data.seek_rsp_data_len = seek_data_len;

    sample_at_log_print("%s data.seek_rsp_data_len = %d\r\n", SLE_UART_SERVER_LOG, data.seek_rsp_data_len);
    sample_at_log_print("%s data.seek_rsp_data: ", SLE_UART_SERVER_LOG);
    for (data_index = 0; data_index<data.seek_rsp_data_len; data_index++) {
        sample_at_log_print("0x%02x ", data.seek_rsp_data[data_index]);
    }
    sample_at_log_print("\r\n");

    ret = sle_set_announce_data(adv_handle, &data);
    if (ret == ERRCODE_SLE_SUCCESS) {
        sample_at_log_print("%s set announce data success.\r\n", SLE_UART_SERVER_LOG);
    } else {
        sample_at_log_print("%s set adv param fail.\r\n", SLE_UART_SERVER_LOG);
    }
    return ERRCODE_SLE_SUCCESS;
}

// ================== 5. 广播回调相关实现 ==================
/**
 * @brief 广播使能回调
 * @param announce_id 广播ID
 * @param status 状态
 */
static void sle_announce_enable_cbk(uint32_t announce_id, errcode_t status)
{
    sample_at_log_print("%s sle announce enable callback id:%02x, state:%x\r\n", SLE_UART_SERVER_LOG, announce_id,
        status);
}

/**
 * @brief 广播禁用回调
 * @param announce_id 广播ID
 * @param status 状态
 */
static void sle_announce_disable_cbk(uint32_t announce_id, errcode_t status)
{
    sample_at_log_print("%s sle announce disable callback id:%02x, state:%x\r\n", SLE_UART_SERVER_LOG, announce_id,
        status);
}

/**
 * @brief 广播终止回调
 * @param announce_id 广播ID
 */
static void sle_announce_terminal_cbk(uint32_t announce_id)
{
    sample_at_log_print("%s sle announce terminal callback id:%02x\r\n", SLE_UART_SERVER_LOG, announce_id);
}

// ================== 6. 广播初始化与注册相关实现 ==================
/**
 * @brief 注册广播相关回调
 * @return 0成功，其他失败
 */
errcode_t sle_uart_announce_register_cbks(void)
{
    errcode_t ret = 0;
    sle_announce_seek_callbacks_t seek_cbks = {0};
    seek_cbks.announce_enable_cb = sle_announce_enable_cbk;
    seek_cbks.announce_disable_cb = sle_announce_disable_cbk;
    seek_cbks.announce_terminal_cb = sle_announce_terminal_cbk;
    ret = sle_announce_seek_register_callbacks(&seek_cbks);
    if (ret != ERRCODE_SLE_SUCCESS) {
        sample_at_log_print("%s sle_uart_announce_register_cbks,register_callbacks fail :%x\r\n",
            SLE_UART_SERVER_LOG, ret);
        return ret;
    }
    return ERRCODE_SLE_SUCCESS;
}

/**
 * @brief 广播初始化，设置参数、数据并启动广播
 * @return 0成功，其他失败
 */
errcode_t sle_uart_server_adv_init(void)
{
    errcode_t ret;
    sle_set_default_announce_param();
    sle_set_default_announce_data();
    ret = sle_start_announce(SLE_ADV_HANDLE_DEFAULT);
    if (ret != ERRCODE_SLE_SUCCESS) {
        sample_at_log_print("%s sle_uart_server_adv_init,sle_start_announce fail :%x\r\n", SLE_UART_SERVER_LOG, ret);
        return ret;
    }
    return ERRCODE_SLE_SUCCESS;
}
