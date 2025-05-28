/**
 * @file SLE_Distribute_Network_Server.c
 * @brief SLE分布式网络服务端示例代码
 *
 * 本文件实现了基于SLE协议的分布式网络服务端，支持通过BLE向客户端下发WiFi配置信息。
 * 包含服务注册、属性添加、回调注册、通知下发等完整流程。
 */

#include "securec.h"
#include "errcode.h"
#include "osal_addr.h"
#include "sle_common.h"
#include "sle_errcode.h"
#include "sle_ssap_server.h"
#include "sle_connection_manager.h"
#include "sle_device_discovery.h"
#include "../inc/SLE_Distribute_Network_Server_adv.h"
#include "../inc/SLE_Distribute_Network_Server.h"

#include "cmsis_os2.h"
#include "debug_print.h"
#include "soc_osal.h"
#include "app_init.h"
#include "common_def.h"
#include "wifi_device_config.h"

#define OCTET_BIT_LEN 8
#define UUID_LEN_2 2

/**
 * @brief 小端序写2字节宏
 * @param _ptr 目标指针
 * @param data 2字节数据
 */
#define encode2byte_little(_ptr, data)                     \
    do {                                                   \
        *(uint8_t *)((_ptr) + 1) = (uint8_t)((data) >> 8); \
        *(uint8_t *)(_ptr) = (uint8_t)(data);              \
    } while (0)

/* sle server app uuid for sample */
static char g_sle_uuid_app_uuid[UUID_LEN_2] = {0x0, 0x0};
/* server property value for sample */
static char g_sle_property_value[OCTET_BIT_LEN] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
/* sle connect id，记录当前连接ID */
static uint16_t g_conn_id = 0;
/* sle server id，注册服务端时分配 */
static uint8_t g_server_id = 0;
/* sle service handle，服务句柄 */
static uint16_t g_service_handle = 0;
/* sle ntf property handle，特征值句柄 */
static uint16_t g_property_handle = 0;

/**
 * @brief 通过handle向client发送notify的内部函数声明
 */
static errcode_t example_sle_server_send_notify_by_handle(const uint8_t *data, uint8_t len);

#define NTF_IND_FLAG_MAX_LEN 17 /* 长度包含字符串结尾的'\0' */

/**
 * @brief WiFi AP SSID和密码的通知/指示信息结构体
 * 用于服务端向客户端发送WiFi配置信息
 * SLE_Distribute_Network_Client侧有定义相同的结构体
 */
typedef struct {
    uint8_t flag[NTF_IND_FLAG_MAX_LEN]; /*!< 标记，包含字符串结尾的'\0' */
    uint8_t ssid_len;                   /*!< SSID长度，包含字符串结尾的'\0' */
    uint8_t key_len;                    /*!< 密码长度，包含字符串结尾的'\0' */
    uint8_t ssid[WIFI_MAX_SSID_LEN];    /*!< SSID内容，包含字符串结尾的'\0' */
    uint8_t key[WIFI_MAX_KEY_LEN];      /*!< 密码内容，包含字符串结尾的'\0' */
} example_wifi_ssid_key_ntf_ind_t;

#define WIFI_SSID_KEY_FLAG "WIFI_SSID_KEY"
#define WIFI_SSID_LEN_BYTES 1
#define WIFI_KEY_LEN_BYTES 1
static char g_ssid[] = "QQ"; /* 第三方WiFi热点的SSID */
static char g_key[] = "tangyuan"; /* 第三方WiFi热点的接入密码 */

/**
 * @brief 处理客户端写入请求，判断是否需要下发WiFi信息
 * @param write_cb_para 写请求参数
 */
static void example_network_info_write_request_cbk(ssaps_req_write_cb_t *write_cb_para)
{
    errcode_t ret = ERRCODE_FAIL;
    uint8_t flag_len = 0;
    uint8_t data_len = 0;
    example_wifi_ssid_key_ntf_ind_t *wifi_ssid_key = NULL;

    flag_len = strlen(WIFI_SSID_KEY_FLAG); /* CLient端发送写请求时，数据不是以'\0'结尾 */
    if (write_cb_para->length == flag_len &&
        memcmp((void *)write_cb_para->value, (void *)WIFI_SSID_KEY_FLAG, flag_len) == 0) {

        data_len = sizeof(example_wifi_ssid_key_ntf_ind_t);

        wifi_ssid_key = osal_vmalloc(sizeof(example_wifi_ssid_key_ntf_ind_t));
        if (wifi_ssid_key == NULL) {
            PRINT("[SLE Server] wifi ssid key notify data mem fail\r\n");
            return;
        }

        if (memset_s(wifi_ssid_key, data_len, 0, data_len) != EOK) {
            PRINT("[SLE Server] wifi ssid key notify data mem set fail\r\n");
            return;
        }

        if (memcpy_s(wifi_ssid_key->flag, NTF_IND_FLAG_MAX_LEN, WIFI_SSID_KEY_FLAG, flag_len) != EOK) {
            PRINT("[SLE Server] wifi ssid flag mem cpy fail\r\n");
            osal_vfree(wifi_ssid_key);
            return;
        }

        wifi_ssid_key->ssid_len = sizeof(g_ssid);
        wifi_ssid_key->key_len = sizeof(g_key);

        if (memcpy_s(wifi_ssid_key->ssid, WIFI_MAX_SSID_LEN, g_ssid, sizeof(g_ssid)) !=
            EOK) { /* 本sample中约定：SSID以'\0'结尾 */
            PRINT("[SLE Server] wifi ssid mem cpy fail\r\n");
            osal_vfree(wifi_ssid_key);
            return;
        }

        if (memcpy_s(wifi_ssid_key->key, WIFI_MAX_KEY_LEN, g_key, sizeof(g_key)) !=
            EOK) { /* 本sample中约定：KEY以'\0'结尾 */
            PRINT("[SLE Server] wifi key mem cpy fail\r\n");
            osal_vfree(wifi_ssid_key);
            return;
        }

        ret = example_sle_server_send_notify_by_handle((uint8_t *)wifi_ssid_key, data_len);
        if (ret != ERRCODE_SUCC) {
            PRINT("[SLE Server] notify wifi ssid and key to client fail. ret 0x%x \r\n", ret);
        }

        osal_vfree(wifi_ssid_key);
    }
}

/**
 * @brief SLE服务UUID基地址
 */
static uint8_t sle_uuid_base[] = {0x37, 0xBE, 0xA8, 0x80, 0xFC, 0x70, 0x11, 0xEA,
                                  0xB7, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

/**
 * @brief 设置SLE UUID基地址
 * @param out 输出UUID结构体
 */
static void example_sle_uuid_set_base(sle_uuid_t *out)
{
    (void)memcpy_s(out->uuid, SLE_UUID_LEN, sle_uuid_base, SLE_UUID_LEN);
    out->len = UUID_LEN_2;
}

/**
 * @brief 设置SLE UUID的低2字节
 * @param u2 低2字节
 * @param out 输出UUID结构体
 */
static void example_sle_uuid_setu2(uint16_t u2, sle_uuid_t *out)
{
    example_sle_uuid_set_base(out);
    out->len = UUID_LEN_2;
    encode2byte_little(&out->uuid[14], u2);
}

/**
 * @brief 读请求回调
 * @param server_id 服务ID
 * @param conn_id 连接ID
 * @param read_cb_para 读请求参数
 * @param status 状态码
 */
static void example_ssaps_read_request_cbk(uint8_t server_id,
                                           uint16_t conn_id,
                                           ssaps_req_read_cb_t *read_cb_para,
                                           errcode_t status)
{
    PRINT("[SLE Server] ssaps read request cbk server_id:0x%x, conn_id:0x%x, handle:0x%x, type:0x%x, status:0x%x\r\n",
          server_id, conn_id, read_cb_para->handle, read_cb_para->type, status);
}

/**
 * @brief 写请求回调
 * @param server_id 服务ID
 * @param conn_id 连接ID
 * @param write_cb_para 写请求参数
 * @param status 状态码
 */
static void example_ssaps_write_request_cbk(uint8_t server_id,
                                            uint16_t conn_id,
                                            ssaps_req_write_cb_t *write_cb_para,
                                            errcode_t status)
{
    PRINT("[SLE Server] ssaps write request cbk server_id:0x%x, conn_id:0x%x, handle:0x%x, status:0x%x\r\n", server_id,
          conn_id, write_cb_para->handle, status);

    for (uint16_t idx = 0; idx < write_cb_para->length; idx++) {
        PRINT("[SLE Server] write request cbk[0x%x] 0x%02x\r\n", idx, write_cb_para->value[idx]);
    }

    if (status == ERRCODE_SUCC) {
        example_network_info_write_request_cbk(write_cb_para);
    }
}

/**
 * @brief MTU变化回调
 * @param server_id 服务ID
 * @param conn_id 连接ID
 * @param mtu_size MTU信息
 * @param status 状态码
 */
static void example_ssaps_mtu_changed_cbk(uint8_t server_id,
                                          uint16_t conn_id,
                                          ssap_exchange_info_t *mtu_size,
                                          errcode_t status)
{
    PRINT("[SLE Server] ssaps mtu changed cbk server_id:0x%x, conn_id:0x%x, mtu_size:0x%x, status:0x%x\r\n", server_id,
          conn_id, mtu_size->mtu_size, status);
}

/**
 * @brief 服务启动回调
 * @param server_id 服务ID
 * @param handle 服务句柄
 * @param status 状态码
 */
static void example_ssaps_start_service_cbk(uint8_t server_id, uint16_t handle, errcode_t status)
{
    PRINT("[SLE Server] start service cbk server_id:0x%x, handle:0x%x, status:0x%x\r\n", server_id, handle, status);
}

/**
 * @brief 注册SSAP Server相关回调
 * @return 错误码
 */
static errcode_t example_sle_ssaps_register_cbks(void)
{
    ssaps_callbacks_t ssaps_cbk = {0};
    ssaps_cbk.start_service_cb = example_ssaps_start_service_cbk;
    ssaps_cbk.mtu_changed_cb = example_ssaps_mtu_changed_cbk;
    ssaps_cbk.read_request_cb = example_ssaps_read_request_cbk;
    ssaps_cbk.write_request_cb = example_ssaps_write_request_cbk;
    return ssaps_register_callbacks(&ssaps_cbk);
}

/**
 * @brief 添加SLE服务
 * @return 错误码
 */
static errcode_t example_sle_server_service_add(void)
{
    errcode_t ret = ERRCODE_FAIL;
    sle_uuid_t service_uuid = {0};
    example_sle_uuid_setu2(SLE_UUID_SERVER_SERVICE, &service_uuid);
    ret = ssaps_add_service_sync(g_server_id, &service_uuid, true, &g_service_handle);
    if (ret != ERRCODE_SUCC) {
        PRINT("[SLE Server] sle uuid add service fail, ret:0x%x\r\n", ret);
        return ERRCODE_FAIL;
    }

    PRINT("[SLE Server] sle uuid add service service_handle: %u\r\n", g_service_handle);

    return ERRCODE_SUCC;
}

/**
 * @brief 添加SLE属性（特征值）及描述符
 * @return 错误码
 */
static errcode_t example_sle_server_property_add(void)
{
    errcode_t ret = ERRCODE_FAIL;
    ssaps_property_info_t property = {0};
    ssaps_desc_info_t descriptor = {0};
    uint8_t ntf_value[] = {0x01, 0x0};

    property.permissions = SSAP_PERMISSION_READ | SSAP_PERMISSION_WRITE;
    example_sle_uuid_setu2(SLE_UUID_SERVER_PROPERTY, &property.uuid);
    property.value = osal_vmalloc(sizeof(g_sle_property_value));
    if (property.value == NULL) {
        PRINT("[SLE Server] sle property mem fail\r\n");
        return ERRCODE_MALLOC;
    }

    if (memcpy_s(property.value, sizeof(g_sle_property_value), g_sle_property_value, sizeof(g_sle_property_value)) !=
        EOK) {
        osal_vfree(property.value);
        PRINT("[SLE Server] sle property mem cpy fail\r\n");
        return ERRCODE_MEMCPY;
    }
    ret = ssaps_add_property_sync(g_server_id, g_service_handle, &property, &g_property_handle);
    if (ret != ERRCODE_SUCC) {
        PRINT("[SLE Server] sle uuid add property fail, ret:0x%x\r\n", ret);
        osal_vfree(property.value);
        return ERRCODE_FAIL;
    }

    PRINT("[SLE Server] sle uuid add property property_handle: %u\r\n", g_property_handle);
    descriptor.permissions = SSAP_PERMISSION_READ | SSAP_PERMISSION_WRITE;
    descriptor.operate_indication = SSAP_OPERATE_INDICATION_BIT_READ | SSAP_OPERATE_INDICATION_BIT_WRITE;
    descriptor.type = SSAP_DESCRIPTOR_USER_DESCRIPTION;
    descriptor.value = ntf_value;
    descriptor.value_len = sizeof(ntf_value);
    if (descriptor.value == NULL) {
        PRINT("[SLE Server] sle descriptor mem fail\r\n");
        osal_vfree(property.value);
        return ERRCODE_MALLOC;
    }
    if (memcpy_s(descriptor.value, sizeof(ntf_value), ntf_value, sizeof(ntf_value)) != EOK) {
        PRINT("[SLE Server] sle descriptor mem cpy fail\r\n");
        osal_vfree(property.value);
        osal_vfree(descriptor.value);
        return ERRCODE_MEMCPY;
    }
    ret = ssaps_add_descriptor_sync(g_server_id, g_service_handle, g_property_handle, &descriptor);
    if (ret != ERRCODE_SUCC) {
        PRINT("[SLE Server] sle uuid add descriptor fail, ret:0x%x\r\n", ret);
        osal_vfree(property.value);
        osal_vfree(descriptor.value);
        return ERRCODE_FAIL;
    }
    osal_vfree(property.value);
    osal_vfree(descriptor.value);
    return ERRCODE_SUCC;
}

/**
 * @brief 注册Server，添加Service和Property，并启动Service
 * @return 错误码
 */
static errcode_t example_sle_server_add(void)
{
    errcode_t ret = ERRCODE_FAIL;
    sle_uuid_t app_uuid = {0};

    PRINT("[SLE Server] sle uuid add service in\r\n");
    app_uuid.len = sizeof(g_sle_uuid_app_uuid);
    if (memcpy_s(app_uuid.uuid, app_uuid.len, g_sle_uuid_app_uuid, sizeof(g_sle_uuid_app_uuid)) != EOK) {
        return ERRCODE_MEMCPY;
    }
    ssaps_register_server(&app_uuid, &g_server_id);

    if (example_sle_server_service_add() != ERRCODE_SUCC) {
        ssaps_unregister_server(g_server_id);
        return ERRCODE_FAIL;
    }

    if (example_sle_server_property_add() != ERRCODE_SUCC) {
        ssaps_unregister_server(g_server_id);
        return ERRCODE_FAIL;
    }
    PRINT("[SLE Server] sle uuid add service, server_id:0x%x, service_handle:0x%x, property_handle:0x%x\r\n",
          g_server_id, g_service_handle, g_property_handle);
    ret = ssaps_start_service(g_server_id, g_service_handle);
    if (ret != ERRCODE_SUCC) {
        PRINT("[SLE Server] sle uuid add service fail, ret:0x%x\r\n", ret);
        return ERRCODE_FAIL;
    }
    PRINT("[SLE Server] sle uuid add service out\r\n");
    return ERRCODE_SUCC;
}

/**
 * @brief server通过handle向client发送数据：notify
 * @param data 通知数据
 * @param len 数据长度
 * @return 错误码
 */
static errcode_t example_sle_server_send_notify_by_handle(const uint8_t *data, uint8_t len)
{
    ssaps_ntf_ind_t param = {0};

    param.handle = g_property_handle;
    param.type = 0;

    param.value = osal_vmalloc(len);
    param.value_len = len;
    if (param.value == NULL) {
        PRINT("[SLE Server] send notify mem fail\r\n");
        return ERRCODE_MALLOC;
    }

    if (memcpy_s(param.value, param.value_len, data, len) != EOK) {
        PRINT("[SLE Server] send input notify memcpy fail\r\n");
        osal_vfree(param.value);
        return ERRCODE_MEMCPY;
    }

    if (ssaps_notify_indicate(g_server_id, g_conn_id, &param) != ERRCODE_SUCC) {
        PRINT("[SLE Server] ssaps notify indicate fail\r\n");
        osal_vfree(param.value);
        return ERRCODE_FAIL;
    }
    osal_vfree(param.value);
    return ERRCODE_SUCC;
}

/**
 * @brief 连接状态变化回调
 * @param conn_id 连接ID
 * @param addr 设备地址
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
    PRINT(
        "[SLE Server] connect state changed conn_id:0x%02x, conn_state:0x%x, pair_state:0x%x, \
        disc_reason:0x%x\r\n",
        conn_id, conn_state, pair_state, disc_reason);
    PRINT("[SLE Server] connect state changed addr:%02x:**:**:**:%02x:%02x\r\n", addr->addr[0], addr->addr[4],
          addr->addr[5]);
    g_conn_id = conn_id;
}

/**
 * @brief 配对完成回调
 * @param conn_id 连接ID
 * @param addr 设备地址
 * @param status 状态码
 */
static void example_sle_pair_complete_cbk(uint16_t conn_id, const sle_addr_t *addr, errcode_t status)
{
    PRINT("[SLE Server] pair complete conn_id:0x%02x, status:0x%x\r\n", conn_id, status);
    PRINT("[SLE Server] pair complete addr:%02x:**:**:**:%02x:%02x\r\n", addr->addr[0], addr->addr[4], addr->addr[5]);
}

/**
 * @brief 注册连接管理相关回调
 * @return 错误码
 */
static errcode_t example_sle_conn_register_cbks(void)
{
    sle_connection_callbacks_t conn_cbks = {0};
    conn_cbks.connect_state_changed_cb = example_sle_connect_state_changed_cbk;
    conn_cbks.pair_complete_cb = example_sle_pair_complete_cbk;
    return sle_connection_register_callbacks(&conn_cbks);
}

/**
 * @brief SLE分布式网络服务主任务
 * 负责SLE服务端的初始化、注册、广播等流程
 * @param arg 任务参数
 * @return 0成功，-1失败
 */
static int example_sle_distribute_network_server_task(const char *arg)
{
    unused(arg);

    (void)osal_msleep(5000); /* 延时5s，等待SLE初始化完毕 */

    PRINT("[SLE Server] try enable.\r\n");
    /* 使能SLE */
    if (enable_sle() != ERRCODE_SUCC) {
        PRINT("[SLE Server] sle enbale fail !\r\n");
        return -1;
    }

    /* 注册连接管理回调函数 */
    if (example_sle_conn_register_cbks() != ERRCODE_SUCC) {
        PRINT("[SLE Server] sle conn register cbks fail !\r\n");
        return -1;
    }

    /* 注册 SSAP server 回调函数 */
    if (example_sle_ssaps_register_cbks() != ERRCODE_SUCC) {
        PRINT("[SLE Server] sle ssaps register cbks fail !\r\n");
        return -1;
    }

    /* 注册Server, 添加Service和property, 启动Service */
    if (example_sle_server_add() != ERRCODE_SUCC) {
        PRINT("[SLE Server] sle server add fail !\r\n");
        return -1;
    }

    /* 设置设备公开，并公开设备 */
    if (example_sle_server_adv_init() != ERRCODE_SUCC) {
        PRINT("[SLE Server] sle server adv fail !\r\n");
        return -1;
    }

    PRINT("[SLE Server] init ok\r\n");

    return 0;
}

#define SLE_DISTRIBUTE_NETWORK_SER_TASK_PRIO 24
#define SLE_DISTRIBUTE_NETWORK_SER_STACK_SIZE 0x2000

/**
 * @brief SLE分布式网络服务入口，创建主任务
 */
static void example_sle_distribute_network_server_entry(void)
{
    osal_task *task_handle = NULL;

    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)example_sle_distribute_network_server_task, 0,
                                      "SLEDistributeNetworkServerTask", SLE_DISTRIBUTE_NETWORK_SER_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, SLE_DISTRIBUTE_NETWORK_SER_TASK_PRIO);
        osal_kfree(task_handle);
    }
    osal_kthread_unlock();
}

/* 启动SLE分布式网络服务入口 */
app_run(example_sle_distribute_network_server_entry);