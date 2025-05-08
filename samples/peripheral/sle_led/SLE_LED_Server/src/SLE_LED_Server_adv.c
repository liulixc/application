// ================== 头文件与宏定义 ==================
#include "securec.h"
#include "errcode.h"
#include "osal_addr.h"
#include "sle_common.h"
#include "sle_device_discovery.h"
#include "sle_errcode.h"
#include "../inc/SLE_LED_Server_adv.h"

#include "cmsis_os2.h"
#include "debug_print.h"







// ================== SLE_LED_Server_adv.c 广播相关实现 ==================
/**
 * 本文件实现了SLE LED Server的广播参数、广播数据、扫描响应数据的配置，
 * 以及广播相关回调的注册和初始化。
 * 主要功能：
 * 1. 设置广播参数 example_sle_set_default_announce_param
 * 2. 设置广播数据 example_sle_set_default_announce_data
 * 3. 设置扫描响应数据 example_sle_set_scan_response_data
 * 4. 注册广播相关回调 example_sle_announce_register_cbks
 * 5. 广播初始化 example_sle_server_adv_init
 */

//可修改 广播名称sle_local_name，本地地址g_sle_local_addr，本地设备名称g_local_device_name

/* sle device name 最大长度 */
#define NAME_MAX_LENGTH 15 // 设备名最大长度
/* 连接调度最小间隔12.5ms，单位125us */
#define SLE_CONN_INTV_MIN_DEFAULT 0x64 // 连接最小间隔（12.5ms）
/* 连接调度最大间隔12.5ms，单位125us */
#define SLE_CONN_INTV_MAX_DEFAULT 0x64 // 连接最大间隔（12.5ms）
/* 广播最小间隔25ms，单位125us */
#define SLE_ADV_INTERVAL_MIN_DEFAULT 0xC8 // 广播最小间隔（25ms）
/* 广播最大间隔25ms，单位125us */
#define SLE_ADV_INTERVAL_MAX_DEFAULT 0xC8 // 广播最大间隔（25ms）
/* 连接超时时间5000ms，单位10ms */
#define SLE_CONN_SUPERVISION_TIMEOUT_DEFAULT 0x1F4 // 连接超时（5000ms）
/* 最大连接延迟，4990ms，单位10ms */
#define SLE_CONN_MAX_LATENCY 0x1F3 // 最大连接延迟（4990ms）
/* 广播发送功率，单位dBm */
#define SLE_ADV_TX_POWER 10 // 广播发送功率
/* 广播句柄ID */
#define SLE_ADV_HANDLE_DEFAULT 1 // 广播句柄
/* 最大广播数据长度 */
#define SLE_ADV_DATA_LEN_MAX 251 // 广播数据最大长度
/* 广播名称，SLE_LED_Server */
static uint8_t sle_local_name[NAME_MAX_LENGTH] = {'S', 'L', 'E', '_', 'L', 'E', 'D', '_',
                                                  'S', 'E', 'R', 'V', 'E', 'R', '\0'};// SLE_LED_Server









// ================== 全局变量定义 ==================

// 0x04 : Demo No. , from 1; 0x01, 0x06, 0x08 : Hoperun Address; 0x06, 0x03 : WS63
static uint8_t g_sle_local_addr[SLE_ADDR_LEN] = {0x04, 0x01, 0x06, 0x08, 0x06, 0x03};

/* 本地设备名称（小写） */
static uint8_t g_local_device_name[] = {'s', 'l', 'e', '_', 'l', 'e', 'd', '_', 's', 'e', 'r', 'v', 'e', 'r'};//sle_led_server








// ================== 静态函数声明 ==================

/**
 * @brief 设置广播包中的本地名称字段
 * @param adv_data 广播数据缓冲区
 * @param max_len 缓冲区最大长度
 * @return 实际写入的长度
 */
static uint16_t example_sle_set_adv_local_name(uint8_t *adv_data, uint16_t max_len);

/**
 * @brief 设置广播包中的发现等级和接入模式字段
 * @param adv_data 广播数据缓冲区
 * @return 实际写入的长度
 */
static uint16_t example_sle_set_adv_data(uint8_t *adv_data);

/**
 * @brief 设置扫描响应数据，包括发射功率和本地名称
 * @param scan_rsp_data 扫描响应数据缓冲区
 * @return 实际写入的长度
 */
static uint16_t example_sle_set_scan_response_data(uint8_t *scan_rsp_data);

/**
 * @brief 设置本地SLE地址
 */
static void example_sle_set_addr(void);

/**
 * @brief 设置SLE本地名称
 */
static void example_sle_set_name(void);

/**
 * @brief 设置默认广播参数
 * @return 设置结果
 */
static errcode_t example_sle_set_default_announce_param(void);

/**
 * @brief 设置默认广播数据和扫描响应数据
 * @return 设置结果
 */
static errcode_t example_sle_set_default_announce_data(void);

/**
 * @brief 广播使能回调
 * @param announce_id 广播ID
 * @param status 状态
 */
void example_sle_announce_enable_cbk(uint32_t announce_id, errcode_t status);

/**
 * @brief 广播关闭回调
 * @param announce_id 广播ID
 * @param status 状态
 */
void example_sle_announce_disable_cbk(uint32_t announce_id, errcode_t status);

/**
 * @brief 广播终止回调
 * @param announce_id 广播ID
 */
void example_sle_announce_terminal_cbk(uint32_t announce_id);

/**
 * @brief SLE使能回调
 * @param status 状态
 */
void example_sle_enable_cbk(errcode_t status);

/**
 * @brief 注册广播相关回调函数
 */
void example_sle_announce_register_cbks(void);

/**
 * @brief SLE服务端广播初始化
 * @return 执行结果
 */
errcode_t example_sle_server_adv_init(void);











// ================== 广播数据与参数设置相关实现 ==================



/**
 * @brief 设置本地SLE地址
 */
static void example_sle_set_addr(void)
{
    uint8_t *addr = g_sle_local_addr;

    sle_addr_t sle_addr = {0};
    sle_addr.type = 0;
    if (memcpy_s(sle_addr.addr, SLE_ADDR_LEN, addr, SLE_ADDR_LEN) != EOK) {
        PRINT("[SLE Adv] addr memcpy fail \r\n");
    }

    if (sle_set_local_addr(&sle_addr) == ERRCODE_SUCC) {
        PRINT("[SLE Adv] set sle addr SUCC \r\n");
    }
}

/**
 * @brief 设置SLE本地名称
 */
static void example_sle_set_name(void)
{
    errcode_t ret = ERRCODE_SUCC;
    ret = sle_set_local_name(g_local_device_name, sizeof(g_local_device_name));
    if (ret != ERRCODE_SUCC) {
        PRINT("[SLE Adv] set local name fail, ret:%x\r\n", ret);
    }
}

/**
 * @brief 设置默认广播参数
 * @return 设置结果
 */
static errcode_t example_sle_set_default_announce_param(void)
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

    if (memcpy_s(param.own_addr.addr, SLE_ADDR_LEN, g_sle_local_addr, SLE_ADDR_LEN) != EOK) {
        PRINT("[SLE Adv] set sle adv param addr memcpy fail\r\n");
        return ERRCODE_MEMCPY;
    }

    return sle_set_announce_param(param.announce_handle, &param);
}


/**
 * @brief 设置广播包中的发现等级和接入模式字段
 * @param adv_data 广播数据缓冲区
 * @return 实际写入的长度
 */
static uint16_t example_sle_set_adv_data(uint8_t *adv_data)
{
    size_t len = 0;
    uint16_t idx = 0;
    errno_t ret = 0;

    // 填充发现等级字段
    len = sizeof(struct sle_adv_common_value);
    struct sle_adv_common_value adv_disc_level = {
        .length = len - 1,
        .type = SLE_ADV_DATA_TYPE_DISCOVERY_LEVEL,
        .value = SLE_ANNOUNCE_LEVEL_NORMAL,
    };
    ret = memcpy_s(&adv_data[idx], SLE_ADV_DATA_LEN_MAX - idx, &adv_disc_level, len);
    if (ret != EOK) {
        PRINT("[SLE Adv] adv_disc_level memcpy fail\r\n");
        return 0;
    }
    idx += len;

    // 填充接入模式字段
    len = sizeof(struct sle_adv_common_value);
    struct sle_adv_common_value adv_access_mode = {
        .length = len - 1,
        .type = SLE_ADV_DATA_TYPE_ACCESS_MODE,
        .value = 0,
    };
    ret = memcpy_s(&adv_data[idx], SLE_ADV_DATA_LEN_MAX - idx, &adv_access_mode, len);
    if (ret != EOK) {
        PRINT("[SLE Adv] memcpy fail\r\n");
        return 0;
    }
    idx += len;
    return idx;
}


/**
 * @brief 设置广播包中的本地名称字段
 * @param adv_data 广播数据缓冲区
 * @param max_len 缓冲区最大长度
 * @return 实际写入的长度
 */
static uint16_t example_sle_set_adv_local_name(uint8_t *adv_data, uint16_t max_len)
{
    errno_t ret = -1;
    uint8_t index = 0;

    uint8_t *local_name = sle_local_name;
    uint8_t local_name_len = (uint8_t)strlen((char *)local_name);
    for (uint8_t i = 0; i < local_name_len; i++) {
        PRINT("[SLE Adv] local_name[%d] = 0x%02x\r\n", i, local_name[i]);
    }

    adv_data[index++] = local_name_len + 1; // 长度字段（类型+数据）
    adv_data[index++] = SLE_ADV_DATA_TYPE_COMPLETE_LOCAL_NAME; // 类型字段
    ret = memcpy_s(&adv_data[index], max_len - index, local_name, local_name_len);
    if (ret != EOK) {
        PRINT("[SLE Adv] memcpy fail\r\n");
        return 0;
    }
    return (uint16_t)index + local_name_len;
}

/**
 * @brief 设置扫描响应数据，包括发射功率和本地名称
 * @param scan_rsp_data 扫描响应数据缓冲区
 * @return 实际写入的长度
 */
static uint16_t example_sle_set_scan_response_data(uint8_t *scan_rsp_data)
{
    uint16_t idx = 0;
    errno_t ret = -1;
    size_t scan_rsp_data_len = sizeof(struct sle_adv_common_value);

    // 填充发射功率字段
    struct sle_adv_common_value tx_power_level = {
        .length = scan_rsp_data_len - 1,
        .type = SLE_ADV_DATA_TYPE_TX_POWER_LEVEL,
        .value = SLE_ADV_TX_POWER,
    };
    ret = memcpy_s(scan_rsp_data, SLE_ADV_DATA_LEN_MAX, &tx_power_level, scan_rsp_data_len);
    if (ret != EOK) {
        PRINT("[SLE Adv] sle scan response data memcpy fail\r\n");
        return 0;
    }
    idx += scan_rsp_data_len;

    /* 填充本地名称 */
    idx += example_sle_set_adv_local_name(&scan_rsp_data[idx], SLE_ADV_DATA_LEN_MAX - idx);
    return idx;
}


/**
 * @brief 设置默认广播数据和扫描响应数据
 * @return 设置结果
 */
static errcode_t example_sle_set_default_announce_data(void)
{
    errcode_t ret = ERRCODE_FAIL;
    uint8_t announce_data_len = 0;
    uint8_t seek_data_len = 0;
    sle_announce_data_t data = {0};
    uint8_t adv_handle = SLE_ADV_HANDLE_DEFAULT;
    uint8_t announce_data[SLE_ADV_DATA_LEN_MAX] = {0};
    uint8_t seek_rsp_data[SLE_ADV_DATA_LEN_MAX] = {0};

    PRINT("[SLE Adv] set adv data default\r\n");
    announce_data_len = example_sle_set_adv_data(announce_data);
    data.announce_data = announce_data;
    data.announce_data_len = announce_data_len;

    seek_data_len = example_sle_set_scan_response_data(seek_rsp_data);
    data.seek_rsp_data = seek_rsp_data;
    data.seek_rsp_data_len = seek_data_len;

    ret = sle_set_announce_data(adv_handle, &data);
    if (ret == ERRCODE_SUCC) {
        PRINT("[SLE Adv] set announce data success.");
    } else {
        PRINT("[SLE Adv] set adv param fail.");
    }
    return ERRCODE_SUCC;
}






// ================== 广播回调相关实现 ==================

/**
 * @brief 广播使能回调
 * @param announce_id 广播ID
 * @param status 状态
 */
void example_sle_announce_enable_cbk(uint32_t announce_id, errcode_t status)
{
    PRINT("[SLE Adv] sle announce enable id:%02x, state:%02x\r\n", announce_id, status);
}

/**
 * @brief 广播关闭回调
 * @param announce_id 广播ID
 * @param status 状态
 */
void example_sle_announce_disable_cbk(uint32_t announce_id, errcode_t status)
{
    PRINT("[SLE Adv] sle announce disable id:%02x, state:%02x\r\n", announce_id, status);
}

/**
 * @brief 广播终止回调
 * @param announce_id 广播ID
 */
void example_sle_announce_terminal_cbk(uint32_t announce_id)
{
    PRINT("[SLE Adv] sle announce terminal id:%02x\r\n", announce_id);
}

/**
 * @brief SLE使能回调
 * @param status 状态
 */
void example_sle_enable_cbk(errcode_t status)
{
    PRINT("[SLE Adv] sle enable status:%02x\r\n", status);
}

/**
 * @brief 注册广播相关回调函数
 */
void example_sle_announce_register_cbks(void)
{
    sle_announce_seek_callbacks_t seek_cbks = {0};
    seek_cbks.announce_enable_cb = example_sle_announce_enable_cbk;
    seek_cbks.announce_disable_cb = example_sle_announce_disable_cbk;
    seek_cbks.announce_terminal_cb = example_sle_announce_terminal_cbk;
    seek_cbks.sle_enable_cb = example_sle_enable_cbk;
    sle_announce_seek_register_callbacks(&seek_cbks);
}







// ================== 广播初始化与注册相关实现 ==================

/**
 * @brief SLE服务端广播初始化
 * @return 执行结果
 */
errcode_t example_sle_server_adv_init(void)
{
    PRINT("[SLE Adv] example_sle_server_adv_init in\r\n");
    example_sle_announce_register_cbks(); // 注册回调
    example_sle_set_default_announce_param(); // 设置默认参数
    example_sle_set_default_announce_data(); // 设置默认数据

    example_sle_set_addr(); // 设置本地地址
    example_sle_set_name(); // 设置本地名称

    sle_start_announce(SLE_ADV_HANDLE_DEFAULT); // 启动广播
    PRINT("[SLE Adv] example_sle_server_adv_init out\r\n");
    return ERRCODE_SUCC;
}
