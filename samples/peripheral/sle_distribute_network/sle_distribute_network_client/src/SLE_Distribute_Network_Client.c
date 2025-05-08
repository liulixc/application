/*
# Copyright (C) 2024 HiHope Open Source Organization .
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
 */

/**
 * @file SLE_Distribute_Network_Client.c
 * @brief SLE分布式网络客户端示例代码，实现通过SLE协议获取WiFi热点信息并连接。
 *
 * 主要流程：
 * 1. 启动SLE扫描，发现目标设备。
 * 2. 建立SLE连接并配对。
 * 3. 通过SSAP协议获取WiFi热点的SSID和密码。
 * 4. 连接到指定WiFi热点。
 *
 * 主要涉及SLE设备发现、连接管理、SSAP客户端、WiFi配置等模块。
 */

#include "securec.h"                     // 安全内存操作函数
#include "sle_device_discovery.h"        // SLE设备发现相关接口
#include "sle_connection_manager.h"      // SLE连接管理相关接口
#include "sle_ssap_client.h"             // SLE SSAP客户端相关接口

#include "soc_osal.h"                    // 操作系统抽象层接口
#include "app_init.h"                    // 应用初始化相关接口
#include "common_def.h"                  // 通用定义
#include "debug_print.h"                 // 调试打印接口
#include "wifi_device_config.h"          // WiFi设备配置接口
#include "../inc/WiFi_STA.h"             // WiFi STA模式相关接口

#define SLE_MTU_SIZE_DEFAULT 300         // SLE默认MTU大小
#define SLE_SEEK_INTERVAL_DEFAULT 100    // SLE扫描间隔
#define SLE_SEEK_WINDOW_DEFAULT 100      // SLE扫描窗口
#define UUID_16BIT_LEN 2                 // 16位UUID长度
#define UUID_128BIT_LEN 16               // 128位UUID长度

// SLE回调结构体，全局静态变量
static sle_announce_seek_callbacks_t g_seek_cbk = {0};      // SLE扫描相关回调
static sle_connection_callbacks_t g_connect_cbk = {0};      // SLE连接相关回调
static ssapc_callbacks_t g_ssapc_cbk = {0};                 // SSAP客户端回调
static sle_addr_t g_remote_addr = {0};                      // 远端设备地址
#if 0
static uint16_t g_client_id = 0; /* 此处使用默认的client ID 0 */
#endif
static uint16_t g_conn_id = 0;                              // 当前连接ID
static ssapc_find_service_result_t g_find_service_result = {0}; // 服务发现结果

static void example_sle_start_scan(void);                   // 启动SLE扫描函数声明

// WiFi热点信息缓存
static char g_ssid[WIFI_MAX_SSID_LEN] = {0}; /* 第三方WiFi热点的SSID */
static char g_key[WIFI_MAX_KEY_LEN] = {0};   /* 第三方WiFi热点的接入密码 */
osal_task *wifi_sta_task_handle = NULL;      // WiFi STA任务句柄

/**
 * @brief WiFi STA任务，连接指定的WiFi热点
 * @param arg 未使用
 * @return 0成功，-1失败
 */
static int example_wifi_sta_task(const char *arg)
{
    unused(arg);

    errcode_t ret = 0;

    // 调用WiFi STA连接函数，参数为SSID和密码
    ret = example_sta_function(g_ssid, sizeof(g_ssid), g_key, sizeof(g_key));
    if (ret != ERRCODE_SUCC) {
        return -1;
    }

    return 0;
}

#define WIFI_STA_TASK_PRIO 24                // WiFi STA任务优先级
#define WIFI_STA_TASK_STACK_SIZE 0x2000      // WiFi STA任务栈大小

/**
 * @brief 创建并启动WiFi STA任务
 */
static void example_wifi_sta_entry(void)
{
    osal_kthread_lock(); // 线程安全加锁
    wifi_sta_task_handle =
        osal_kthread_create((osal_kthread_handler)example_wifi_sta_task, 0, "WiFiStaTask", WIFI_STA_TASK_STACK_SIZE);
    if (wifi_sta_task_handle != NULL) {
        osal_kthread_set_priority(wifi_sta_task_handle, WIFI_STA_TASK_PRIO);
        osal_kfree(wifi_sta_task_handle); // 释放任务句柄
    }
    osal_kthread_unlock(); // 解锁
}

#define NTF_IND_FLAG_MAX_LEN 17 /* 长度包含字符串结尾的'\0' */

/**
 * @brief WiFi AP SSID和密码的通知/指示信息结构体
 * @attention SLE_Distribute_Network_Server侧有定义相同的结构体
 */
typedef struct {
    uint8_t flag[NTF_IND_FLAG_MAX_LEN]; /*!< 标记，包含字符串结尾的'\0' */
    uint8_t ssid_len;                   /*!< SSID长度，包含字符串结尾的'\0' */
    uint8_t key_len;                    /*!< 密码长度，包含字符串结尾的'\0' */
    uint8_t ssid[WIFI_MAX_SSID_LEN];    /*!< SSID内容，包含字符串结尾的'\0' */
    uint8_t key[WIFI_MAX_KEY_LEN];      /*!< 密码内容，包含字符串结尾的'\0' */
} example_wifi_ssid_key_ntf_ind_t;

#define WIFI_SSID_KEY_FLAG "WIFI_SSID_KEY"   // WiFi SSID/KEY标识字符串
#define WIFI_SSID_LEN_BYTES 1                // SSID长度字节数
#define WIFI_KEY_LEN_BYTES 1                 // 密码长度字节数

/**
 * @brief 处理WiFi热点信息通知回调
 * @param data SSAP通知数据
 */
static void example_wifi_notification_cbk(ssapc_handle_value_t *data)
{
    uint8_t flag_len = 0;
    example_wifi_ssid_key_ntf_ind_t *wifi_ssid_key = NULL;

    flag_len = sizeof(WIFI_SSID_KEY_FLAG);

    // 判断数据长度和标志位
    if (data->data_len > NTF_IND_FLAG_MAX_LEN &&
        memcmp((void *)data->data, (void *)WIFI_SSID_KEY_FLAG, flag_len) == 0) {

        if (data->data_len != sizeof(example_wifi_ssid_key_ntf_ind_t)) {
            PRINT("[SLE Client] %s data len abnormal! Expected len %u, actual len %u\r\n", WIFI_SSID_KEY_FLAG,
                  sizeof(example_wifi_ssid_key_ntf_ind_t), data->data_len);
            return;
        }

        wifi_ssid_key = (example_wifi_ssid_key_ntf_ind_t *)data->data;

        // 拷贝SSID和密码到全局变量
        if (memcpy_s(g_ssid, WIFI_MAX_SSID_LEN, wifi_ssid_key->ssid, wifi_ssid_key->ssid_len) != EOK) {
            PRINT("[SLE Client] wifi ssid memcpy fail\r\n");
            return;
        }

        if (memcpy_s(g_key, WIFI_MAX_KEY_LEN, wifi_ssid_key->key, wifi_ssid_key->key_len) != EOK) {
            PRINT("[SLE Client] wifi key memcpy fail\r\n");
            return;
        }

        // 启动WiFi STA连接
        example_wifi_sta_entry();
    }
}

/**
 * @brief 通过写请求获取WiFi热点信息
 * @param client_id 客户端ID
 * @param conn_id 连接ID
 * @return 操作结果
 */
static errcode_t example_get_wifi_by_write_req(uint8_t client_id, uint16_t conn_id)
{
    uint8_t write_req_data[] = {'W', 'I', 'F', 'I', '_', 'S', 'S', 'I', 'D', '_', 'K', 'E', 'Y'};
    uint8_t len = sizeof(write_req_data);
    ssapc_write_param_t param = {0};
    param.handle = g_find_service_result.start_hdl;
    param.type = SSAP_PROPERTY_TYPE_VALUE;
    param.data_len = len;
    param.data = write_req_data;
    return ssapc_write_req(client_id, conn_id, &param);
}

/**
 * @brief SLE使能回调，成功后启动扫描
 * @param status 操作结果
 */
static void example_sle_enable_cbk(errcode_t status)
{
    if (status == ERRCODE_SUCC) {
        example_sle_start_scan();
    }
}

/**
 * @brief SLE扫描使能回调
 * @param status 操作结果
 */
static void example_sle_seek_enable_cbk(errcode_t status)
{
    if (status == ERRCODE_SUCC) {
        return;
    }
}

/**
 * @brief SLE扫描禁用回调，成功后连接远端设备
 * @param status 操作结果
 */
static void example_sle_seek_disable_cbk(errcode_t status)
{
    if (status == ERRCODE_SUCC) {
        sle_connect_remote_device(&g_remote_addr);
    }
}

// 期望发现的目标设备地址（需根据实际修改）
static uint8_t g_sle_expected_addr[SLE_ADDR_LEN] = {0x05, 0x01, 0x06, 0x08, 0x06, 0x03};

/**
 * @brief SLE扫描结果回调，查找目标设备
 * @param seek_result_data 扫描结果
 */
static void example_sle_seek_result_info_cbk(sle_seek_result_info_t *seek_result_data)
{
    if (seek_result_data == NULL) {
        PRINT("[SLE Client] seek result seek_result_data is NULL\r\n");
        return;
    }

    // 判断是否为期望的设备地址
    if (memcmp((void *)seek_result_data->addr.addr, (void *)g_sle_expected_addr, SLE_ADDR_LEN) == 0) {
        PRINT("[SLE Client] seek result find expected addr:%02x***%02x%02x\r\n", seek_result_data->addr.addr[0],
              seek_result_data->addr.addr[4], seek_result_data->addr.addr[5]);
        (void)memcpy_s(&g_remote_addr, sizeof(sle_addr_t), &seek_result_data->addr, sizeof(sle_addr_t));
        sle_stop_seek();
    }
}

/**
 * @brief 注册SLE扫描相关回调
 */
static void example_sle_seek_cbk_register(void)
{
    g_seek_cbk.sle_enable_cb = example_sle_enable_cbk;
    g_seek_cbk.seek_enable_cb = example_sle_seek_enable_cbk;
    g_seek_cbk.seek_disable_cb = example_sle_seek_disable_cbk;
    g_seek_cbk.seek_result_cb = example_sle_seek_result_info_cbk;
}

/**
 * @brief SLE连接状态变化回调
 * @param conn_id 连接ID
 * @param addr 远端地址
 * @param conn_state 连接状态
 * @param pair_state 配对状态
 * @param disc_reason 断开原因
 */
static void example_sle_connect_state_changed_cbk(uint16_t conn_id,
                                                  const sle_addr_t *addr,
                                                  sle_acb_state_t conn_state,
                                                  sle_pair_state_t pair_state,
                                                  sle_disc_reason_t disc_reason)
{
    PRINT("[SLE Client] conn state changed conn_id:0x%x, addr:%02x***%02x%02x\r\n", conn_id, addr->addr[0],
          addr->addr[4], addr->addr[5]); /* 0 4 5: addr index */
    PRINT("[SLE Client] conn state changed disc_reason:0x%x\r\n", disc_reason);
    if (conn_state == SLE_ACB_STATE_CONNECTED) {
        if (pair_state == SLE_PAIR_NONE) {
            sle_pair_remote_device(&g_remote_addr);
        }
        g_conn_id = conn_id;
    }
}

/**
 * @brief SLE配对完成回调
 * @param conn_id 连接ID
 * @param addr 远端地址
 * @param status 配对结果
 */
static void example_sle_pair_complete_cbk(uint16_t conn_id, const sle_addr_t *addr, errcode_t status)
{
    PRINT("[SLE Client] pair complete conn_id:0x%x, status:0x%x, addr:%02x***%02x%02x\r\n", conn_id, status,
          addr->addr[0], addr->addr[4], addr->addr[5]); /* 0 4 5: addr index */
    if (status == ERRCODE_SUCC) {
        ssap_exchange_info_t info = {0};
        info.mtu_size = SLE_MTU_SIZE_DEFAULT;
        info.version = 1;
        ssapc_exchange_info_req(1, conn_id, &info); /* 此处没有使用默认的client ID 0 */
    }
}

/**
 * @brief 注册SLE连接相关回调
 */
static void example_sle_connect_cbk_register(void)
{
    g_connect_cbk.connect_state_changed_cb = example_sle_connect_state_changed_cbk;
    g_connect_cbk.pair_complete_cb = example_sle_pair_complete_cbk;
}

/**
 * @brief SSAP交换信息回调，成功后发起服务发现
 * @param client_id 客户端ID
 * @param conn_id 连接ID
 * @param param 交换信息参数
 * @param status 操作结果
 */
static void example_sle_exchange_info_cbk(uint8_t client_id,
                                          uint16_t conn_id,
                                          ssap_exchange_info_t *param,
                                          errcode_t status)
{
    PRINT("[SLE Client] pair complete client id:0x%x, status:0x%x\r\n", client_id, status);
    PRINT("[SLE Client] exchange mtu, mtu size:0x%x, version:0x%x.\r\n", param->mtu_size, param->version);

    if (status == ERRCODE_SUCC) {
        ssapc_find_structure_param_t find_param = {0};
        find_param.type = SSAP_FIND_TYPE_PRIMARY_SERVICE;
        find_param.start_hdl = 1;
        find_param.end_hdl = 0xFFFF;
        ssapc_find_structure(0, conn_id, &find_param); /* 此处使用默认的client ID 0 */
    }
}

/**
 * @brief SSAP服务发现回调
 * @param client_id 客户端ID
 * @param conn_id 连接ID
 * @param service 服务发现结果
 * @param status 操作结果
 */
static void example_sle_find_structure_cbk(uint8_t client_id,
                                           uint16_t conn_id,
                                           ssapc_find_service_result_t *service,
                                           errcode_t status)
{
    PRINT("[SLE Client] find structure cbk client:0x%x conn_id:0x%x status:0x%x \r\n", client_id, conn_id, status);
    PRINT("[SLE Client] find structure start_hdl:[0x%04x], end_hdl:[0x%04x], uuid len:0x%x\r\n", service->start_hdl,
          service->end_hdl, service->uuid.len);
    if (service->uuid.len == UUID_16BIT_LEN) {
        PRINT("[SLE Client] structure uuid:[0x%02x][0x%02x]\r\n", service->uuid.uuid[14],
              service->uuid.uuid[15]); /* 14 15: uuid index */
    } else {
        for (uint8_t idx = 0; idx < UUID_128BIT_LEN; idx++) {
            PRINT("[SLE Client] structure uuid[0x%x]:[0x%02x]\r\n", idx, service->uuid.uuid[idx]);
        }
    }

    if (status == ERRCODE_SUCC) {
        g_find_service_result.start_hdl = service->start_hdl;
        g_find_service_result.end_hdl = service->end_hdl;
        memcpy_s(&g_find_service_result.uuid, sizeof(sle_uuid_t), &service->uuid, sizeof(sle_uuid_t));
    }
}

/**
 * @brief SSAP结构体发现完成回调，发起WiFi信息获取请求
 * @param client_id 客户端ID
 * @param conn_id 连接ID
 * @param structure_result 结构体发现结果
 * @param status 操作结果
 */
static void example_sle_find_structure_cmp_cbk(uint8_t client_id,
                                               uint16_t conn_id,
                                               ssapc_find_structure_result_t *structure_result,
                                               errcode_t status)
{
    PRINT("[SLE Client] find structure cmp cbk client id:0x%x conn_id:0x%x status:0x%x type:%d uuid len:0x%x \r\n",
          client_id, conn_id, status, structure_result->type, structure_result->uuid.len);
    if (structure_result->uuid.len == UUID_16BIT_LEN) {
        PRINT("[SLE Client] find structure cmp cbk structure uuid:[0x%02x][0x%02x]\r\n",
              structure_result->uuid.uuid[14], structure_result->uuid.uuid[15]); /* 14 15: uuid index */
    } else {
        for (uint8_t idx = 0; idx < UUID_128BIT_LEN; idx++) {
            PRINT("[SLE Client] find structure cmp cbk structure uuid[0x%x]:[0x%02x]\r\n", idx,
                  structure_result->uuid.uuid[idx]);
        }
    }

    if (status == ERRCODE_SUCC) {
        if (example_get_wifi_by_write_req(client_id, conn_id) != ERRCODE_SUCC) {
            PRINT("[SLE Client] get wifi ssid and password fail\r\n");
        }
    }
}

/**
 * @brief SSAP属性发现回调
 * @param client_id 客户端ID
 * @param conn_id 连接ID
 * @param property 属性发现结果
 * @param status 操作结果
 */
static void example_sle_find_property_cbk(uint8_t client_id,
                                          uint16_t conn_id,
                                          ssapc_find_property_result_t *property,
                                          errcode_t status)
{
    PRINT(
        "[SLE Client] find property cbk, client id:0x%x, conn id:0x%x, operate ind:0x%x, handle:0x%x"
        "descriptors count:0x%x status:0x%x.\r\n",
        client_id, conn_id, property->operate_indication, property->handle, property->descriptors_count, status);
    for (uint16_t idx = 0; idx < property->descriptors_count; idx++) {
        PRINT("[SLE Client] find property cbk, descriptors type [0x%x]: 0x%02x.\r\n", idx,
              property->descriptors_type[idx]);
    }
    if (property->uuid.len == UUID_16BIT_LEN) {
        PRINT("[SLE Client] find property cbk, uuid: 0x%02x 0x%02x.\r\n", property->uuid.uuid[14],
              property->uuid.uuid[15]); /* 14 15: uuid index */
    } else if (property->uuid.len == UUID_128BIT_LEN) {
        for (uint16_t idx = 0; idx < UUID_128BIT_LEN; idx++) {
            PRINT("[SLE Client] find property cbk, uuid [0x%x]: 0x%02x.\r\n", idx, property->uuid.uuid[idx]);
        }
    }
}

/**
 * @brief SSAP写确认回调
 * @param client_id 客户端ID
 * @param conn_id 连接ID
 * @param write_result 写操作结果
 * @param status 操作结果
 */
static void example_sle_write_cfm_cbk(uint8_t client_id,
                                      uint16_t conn_id,
                                      ssapc_write_result_t *write_result,
                                      errcode_t status)
{
    PRINT("[SLE Client] write cfm cbk client id:0x%x conn_id:0x%x status:0x%x.\r\n", client_id, conn_id, status);
    PRINT("[SLE Client] write cfm cbk handle:0x%x, type:0x%x\r\n", write_result->handle, write_result->type);
}

/**
 * @brief SSAP读确认回调
 * @param client_id 客户端ID
 * @param conn_id 连接ID
 * @param read_data 读数据
 * @param status 操作结果
 */
static void example_sle_read_cfm_cbk(uint8_t client_id,
                                     uint16_t conn_id,
                                     ssapc_handle_value_t *read_data,
                                     errcode_t status)
{
    PRINT("[SLE Client] read cfm cbk client id:0x%x conn id:0x%x status:0x%x\r\n", client_id, conn_id, status);
    PRINT("[SLE Client] read cfm cbk handle:0x%x, type:0x%x, len:0x%x\r\n", read_data->handle, read_data->type,
          read_data->data_len);
    for (uint16_t idx = 0; idx < read_data->data_len; idx++) {
        PRINT("[SLE Client] read cfm cbk[0x%x] 0x%02x\r\n", idx, read_data->data[idx]);
    }
}

/**
 * @brief SSAP通知回调，处理WiFi热点信息
 * @param client_id 客户端ID
 * @param conn_id 连接ID
 * @param data 通知数据
 * @param status 操作结果
 */
static void example_sle_notification_cbk(uint8_t client_id,
                                         uint16_t conn_id,
                                         ssapc_handle_value_t *data,
                                         errcode_t status)
{
    PRINT("[SLE Client] notification cbk client id:0x%x conn id:0x%x status:0x%x\r\n", client_id, conn_id, status);

    PRINT("[SLE Client] notification cbk handle:0x%x, type:0x%x data len:0x%x \r\n", data->handle, data->type,
          data->data_len);

    for (uint16_t idx = 0; idx < data->data_len; idx++) {
        PRINT("[SLE Client] notification cbk[0x%x] 0x%02x\r\n", idx, data->data[idx]);
    }

    if (status == ERRCODE_SUCC) {
        example_wifi_notification_cbk(data);
    }
}

/**
 * @brief SSAP指示回调
 * @param client_id 客户端ID
 * @param conn_id 连接ID
 * @param data 指示数据
 * @param status 操作结果
 */
static void example_sle_indication_cbk(uint8_t client_id,
                                       uint16_t conn_id,
                                       ssapc_handle_value_t *data,
                                       errcode_t status)
{
    PRINT("[SLE Client] indication cbk client id:0x%x conn id:0x%x status:0x%x\r\n", client_id, conn_id, status);

    PRINT("[SLE Client] indication cbk handle:0x%x, type:0x%x data len:0x%x \r\n", data->handle, data->type,
          data->data_len);

    for (uint16_t idx = 0; idx < data->data_len; idx++) {
        PRINT("[SLE Client] indication cbk[0x%x] 0x%02x\r\n", idx, data->data[idx]);
    }
}

/**
 * @brief 注册SSAP客户端相关回调
 */
static void example_sle_ssapc_cbk_register(void)
{
    g_ssapc_cbk.exchange_info_cb = example_sle_exchange_info_cbk;
    g_ssapc_cbk.find_structure_cb = example_sle_find_structure_cbk;
    g_ssapc_cbk.find_structure_cmp_cb = example_sle_find_structure_cmp_cbk;
    g_ssapc_cbk.ssapc_find_property_cbk = example_sle_find_property_cbk;
    g_ssapc_cbk.write_cfm_cb = example_sle_write_cfm_cbk;
    g_ssapc_cbk.read_cfm_cb = example_sle_read_cfm_cbk;

    g_ssapc_cbk.notification_cb = example_sle_notification_cbk;
    g_ssapc_cbk.indication_cb = example_sle_indication_cbk;
}

/**
 * @brief 启动SLE扫描
 */
static void example_sle_start_scan(void)
{
    sle_seek_param_t param = {0};
    param.own_addr_type = 0;
    param.filter_duplicates = 0;
    param.seek_filter_policy = 0;
    param.seek_phys = 1;
    param.seek_type[0] = 0;
    param.seek_interval[0] = SLE_SEEK_INTERVAL_DEFAULT;
    param.seek_window[0] = SLE_SEEK_WINDOW_DEFAULT;
    sle_set_seek_param(&param);
    sle_start_seek();
}

/**
 * @brief SLE分布式网络客户端主任务
 * @param arg 未使用
 * @return 0成功，-1失败
 */
static int example_sle_distribute_network_client_task(const char *arg)
{
    unused(arg);

    (void)osal_msleep(5000); /* 延时5s，等待wifi和SLE初始化完毕 */

    PRINT("[SLE Client] try enable.\r\n");

    // 注册各类回调
    example_sle_seek_cbk_register();
    example_sle_connect_cbk_register();
    example_sle_ssapc_cbk_register();
    if (sle_announce_seek_register_callbacks(&g_seek_cbk) != ERRCODE_SUCC) {
        PRINT("[SLE Client] sle announce seek register callbacks fail !\r\n");
        return -1;
    }

    if (sle_connection_register_callbacks(&g_connect_cbk) != ERRCODE_SUCC) {
        PRINT("[SLE Client] sle connection register callbacks fail !\r\n");
        return -1;
    }

    if (ssapc_register_callbacks(&g_ssapc_cbk) != ERRCODE_SUCC) {
        PRINT("[SLE Client] sle ssapc register callbacks !\r\n");
        return -1;
    }

    if (enable_sle() != ERRCODE_SUCC) {
        PRINT("[SLE Client] sle enbale fail !\r\n");
        return -1;
    }

    return 0;
}

#define SLE_DISTRIBUTE_NETWORK_CLI_TASK_PRIO 24         // SLE客户端任务优先级
#define SLE_DISTRIBUTE_NETWORK_CLI_STACK_SIZE 0x2000    // SLE客户端任务栈大小

/**
 * @brief 创建并启动SLE分布式网络客户端任务
 */
static void example_sle_distribute_network_client_entry(void)
{
    osal_task *task_handle = NULL;

    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)example_sle_distribute_network_client_task, 0,
                                      "SLEDistributeNetworkClientTask", SLE_DISTRIBUTE_NETWORK_CLI_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, SLE_DISTRIBUTE_NETWORK_CLI_TASK_PRIO);
        osal_kfree(task_handle);
    }
    osal_kthread_unlock();
}

/**
 * @brief 应用入口，运行SLE分布式网络客户端
 */
app_run(example_sle_distribute_network_client_entry);