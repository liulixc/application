/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd.. 2023. All rights reserved.
 * Description: sle uuid server sample.
 */
#include "securec.h"
#include "errcode.h"
#include "osal_addr.h"
#include "soc_osal.h"
#include "app_init.h"
#include "common_def.h"
#include "sle_common.h"
#include "sle_errcode.h"
#include "sle_ssap_server.h"
#include "sle_connection_manager.h"
#include "sle_device_discovery.h"
#include "sle_server_adv.h"
#include "sle_uuid_server.h"

#define OCTET_BIT_LEN 8
#define UUID_LEN_2     2
#define BT_INDEX_4     4
#define BT_INDEX_5     5
#define BT_INDEX_0     0

#define encode2byte_little(_ptr, data) \
    do { \
        *(uint8_t *)((_ptr) + 1) = (uint8_t)((data) >> 8); \
        *(uint8_t *)(_ptr) = (uint8_t)(data); \
    } while (0)

/* sle server app uuid for test */
char g_sle_uuid_app_uuid[UUID_LEN_2] = {0x0, 0x0};
/* server notify property uuid for test */
char g_sle_property_value[OCTET_BIT_LEN] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
/* sle connect acb handle */
uint16_t g_sle_conn_hdl = 0;
/* sle server handle */
uint8_t g_server_id = 0;
/* sle service handle */
uint16_t g_service_handle = 0;
/* sle ntf property handle */
uint16_t g_property_handle = 0;

sle_acb_state_t g_sle_hybrids_conn_state = SLE_ACB_STATE_NONE;

static uint8_t sle_uuid_base[] = { 0x37, 0xBE, 0xA8, 0x80, 0xFC, 0x70, 0x11, 0xEA, \
    0xB7, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

static void sle_uuid_set_base(sle_uuid_t *out)
{
    (void)memcpy_s(out->uuid, SLE_UUID_LEN, sle_uuid_base, SLE_UUID_LEN);
    out->len = UUID_LEN_2;
}

static void sle_uuid_setu2(uint16_t u2, sle_uuid_t *out)
{
    sle_uuid_set_base(out);
    out->len = UUID_LEN_2;
    encode2byte_little(&out->uuid[14], u2);
}

static void ssaps_read_request_cbk(uint8_t server_id, uint16_t conn_id, ssaps_req_read_cb_t *read_cb_para,
    errcode_t status)
{
    osal_printk("[uuid server] ssaps read request cbk server_id:%x, conn_id:%x, handle:%x, status:%x\r\n",
        server_id, conn_id, read_cb_para->handle, status);
}

static void ssaps_write_request_cbk(uint8_t server_id, uint16_t conn_id, ssaps_req_write_cb_t *write_cb_para,
    errcode_t status)
{
    osal_printk("[uuid server] ssaps write request cbk server_id:%x, conn_id:%x, handle:%x, status:%x\r\n",
        server_id, conn_id, write_cb_para->handle, status);
    osal_printk("Hybrid-S recv data:%02x %02x %02x %02x",write_cb_para->value[0],write_cb_para->value[1],
        write_cb_para->value[2],write_cb_para->value[3]);
}

static void ssaps_mtu_changed_cbk(uint8_t server_id, uint16_t conn_id,  ssap_exchange_info_t *mtu_size,
    errcode_t status)
{
    osal_printk("[uuid server] ssaps write request cbk server_id:%x, conn_id:%x, mtu_size:%x, status:%x\r\n",
        server_id, conn_id, mtu_size->mtu_size, status);
}

static void ssaps_start_service_cbk(uint8_t server_id, uint16_t handle, errcode_t status)
{
    osal_printk("[uuid server] start service cbk server_id:%x, handle:%x, status:%x\r\n",
        server_id, handle, status);
}

static errcode_t sle_ssaps_register_cbks(void)
{
    errcode_t ret = 0;
    ssaps_callbacks_t ssaps_cbk = {0};
    ssaps_cbk.start_service_cb = ssaps_start_service_cbk;
    ssaps_cbk.mtu_changed_cb = ssaps_mtu_changed_cbk;
    ssaps_cbk.read_request_cb = ssaps_read_request_cbk;
    ssaps_cbk.write_request_cb = ssaps_write_request_cbk;
    ret = ssaps_register_callbacks(&ssaps_cbk);
    if(ret != ERRCODE_SUCC)
    {
        osal_printk("[sle hybridS]: sle server ssap register fail\r\n");
        return ret;
    }
    return ERRCODE_SUCC;
}

static errcode_t sle_uuid_server_service_add(void)
{
    errcode_t ret;
    sle_uuid_t service_uuid = {0};
    sle_uuid_setu2(SLE_UUID_SERVER_SERVICE, &service_uuid);
    ret = ssaps_add_service_sync(g_server_id, &service_uuid, 1, &g_service_handle);
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[uuid server] sle uuid add service fail, ret:%x\r\n", ret);
        return ERRCODE_SLE_FAIL;
    }
    return ERRCODE_SLE_SUCCESS;
}

static errcode_t sle_uuid_server_property_add(void)
{
    errcode_t ret;
    ssaps_property_info_t property = {0};
    ssaps_desc_info_t descriptor = {0};
    uint8_t ntf_value[] = {0x01, 0x0};

    property.permissions = SLE_UUID_TEST_PROPERTIES;
    sle_uuid_setu2(SLE_UUID_SERVER_NTF_REPORT, &property.uuid);
    property.operate_indication = SSAP_OPERATE_INDICATION_BIT_READ | SSAP_OPERATE_INDICATION_BIT_NOTIFY;
    property.value = osal_vmalloc(sizeof(g_sle_property_value));
    if (property.value == NULL) {
        osal_printk("[uuid server] sle property mem fail\r\n");
        return ERRCODE_SLE_FAIL;
    }
    if (memcpy_s(property.value, sizeof(g_sle_property_value), g_sle_property_value,
        sizeof(g_sle_property_value)) != EOK) {
        osal_vfree(property.value);
        osal_printk("[uuid server] sle property mem cpy fail\r\n");
        return ERRCODE_SLE_FAIL;
    }
    ret = ssaps_add_property_sync(g_server_id, g_service_handle, &property,  &g_property_handle);
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[uuid server] sle uuid add property fail, ret:%x\r\n", ret);
        osal_vfree(property.value);
        return ERRCODE_SLE_FAIL;
    }
    descriptor.permissions = SLE_UUID_TEST_DESCRIPTOR;
    descriptor.operate_indication = SSAP_OPERATE_INDICATION_BIT_READ | SSAP_OPERATE_INDICATION_BIT_WRITE;
    descriptor.type = SSAP_DESCRIPTOR_USER_DESCRIPTION;
    descriptor.value = osal_vmalloc(sizeof(ntf_value));
    if (descriptor.value == NULL) {
        osal_printk("[uuid server] sle descriptor mem fail\r\n");
        osal_vfree(property.value);
        return ERRCODE_SLE_FAIL;
    }
    if (memcpy_s(descriptor.value, sizeof(ntf_value), ntf_value, sizeof(ntf_value)) != EOK) {
        osal_printk("[uuid server] sle descriptor mem cpy fail\r\n");
        osal_vfree(property.value);
        osal_vfree(descriptor.value);
        return ERRCODE_SLE_FAIL;
    }
    ret = ssaps_add_descriptor_sync(g_server_id, g_service_handle, g_property_handle, &descriptor);
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[uuid server] sle uuid add descriptor fail, ret:%x\r\n", ret);
        osal_vfree(property.value);
        osal_vfree(descriptor.value);
        return ERRCODE_SLE_FAIL;
    }
    osal_vfree(property.value);
    osal_vfree(descriptor.value);
    return ERRCODE_SLE_SUCCESS;
}

static errcode_t sle_uuid_server_add(void)
{
    errcode_t ret;
    sle_uuid_t app_uuid = {0};

    osal_printk("[uuid server] sle uuid add service in\r\n");
    app_uuid.len = sizeof(g_sle_uuid_app_uuid);
    if (memcpy_s(app_uuid.uuid, app_uuid.len, g_sle_uuid_app_uuid, sizeof(g_sle_uuid_app_uuid)) != EOK) {
        return ERRCODE_SLE_FAIL;
    }
    ssaps_register_server(&app_uuid, &g_server_id);

    if (sle_uuid_server_service_add() != ERRCODE_SLE_SUCCESS) {
        ssaps_unregister_server(g_server_id);
        return ERRCODE_SLE_FAIL;
    }

    if (sle_uuid_server_property_add() != ERRCODE_SLE_SUCCESS) {
        ssaps_unregister_server(g_server_id);
        return ERRCODE_SLE_FAIL;
    }
    osal_printk("[uuid server] sle uuid add service, server_id:%x, service_handle:%x, property_handle:%x\r\n",
        g_server_id, g_service_handle, g_property_handle);
    ret = ssaps_start_service(g_server_id, g_service_handle);
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[uuid server] sle uuid add service fail, ret:%x\r\n", ret);
        return ERRCODE_SLE_FAIL;
    }
    osal_printk("[uuid server] sle uuid add service out\r\n");
    return ERRCODE_SLE_SUCCESS;
}

/* device通过uuid向host发送数据：report */
errcode_t sle_uuid_server_send_report_by_uuid(const uint8_t *data, uint16_t len)
{
    ssaps_ntf_ind_by_uuid_t param = {0};
    param.type = SSAP_PROPERTY_TYPE_VALUE;
    param.start_handle = g_service_handle;
    param.end_handle = g_property_handle;
    param.value_len = len;
    param.value = osal_vmalloc(len);
    if (param.value == NULL) {
        osal_printk("[uuid server] send report new fail\r\n");
        return ERRCODE_SLE_FAIL;
    }
    if (memcpy_s(param.value, param.value_len, data, len) != EOK) {
        osal_printk("[uuid server] send input report memcpy fail\r\n");
        osal_vfree(param.value);
        return ERRCODE_SLE_FAIL;
    }
    sle_uuid_setu2(SLE_UUID_SERVER_NTF_REPORT, &param.uuid);
    ssaps_notify_indicate_by_uuid(g_server_id, g_sle_conn_hdl, &param);
    osal_vfree(param.value);
    return ERRCODE_SLE_SUCCESS;
}


/* device通过handle向host发送数据：report */
errcode_t sle_uuid_server_send_report_by_handle(const uint8_t *data, uint8_t len)
{
    ssaps_ntf_ind_t param = {0};
    errcode_t ret = 0;
    param.handle = g_property_handle;
    param.type = SSAP_PROPERTY_TYPE_VALUE;
    param.value = osal_vmalloc(len);
    param.value_len = len;
    if (param.value == NULL) {
        osal_printk("[uuid server] send report new fail\r\n");
        return ERRCODE_SLE_FAIL;
    }
    if (memcpy_s(param.value, param.value_len, data, len) != EOK) {
        osal_printk("[uuid server] send input report memcpy fail\r\n");
        osal_vfree(param.value);
        return ERRCODE_SLE_FAIL;
    }
    ret = ssaps_notify_indicate(g_server_id, g_sle_conn_hdl, &param);
    if(ret != ERRCODE_SUCC)
    {
        osal_printk("[hybrid server] ssaps_notify_indicate FAIL\r\n");
        osal_vfree(param.value);
        return ret;
    }
    osal_vfree(param.value);
    return ERRCODE_SLE_SUCCESS;
}

void sle_server_connect_state_changed_cbk(uint16_t conn_id, const sle_addr_t *addr,
    sle_acb_state_t conn_state, sle_pair_state_t pair_state, sle_disc_reason_t disc_reason)
{
    g_sle_hybrids_conn_state = conn_state;
    osal_printk("[uuid server] connect state changed conn_id:0x%02x, conn_state:0x%x, pair_state:0x%x, \
        disc_reason:0x%x\r\n", conn_id, conn_state, pair_state, disc_reason);
    osal_printk("[sle_server_connect_state_changed_cbk] addr:%02x:%02x:%02x:%02x:%02x:%02x\r\n",
           addr->addr[0], addr->addr[1], addr->addr[2], addr->addr[3],
           addr->addr[4], addr->addr[5]);
    
    if (conn_state == SLE_ACB_STATE_CONNECTED)
    {
        g_sle_conn_hdl = conn_id;
    }

    else if (conn_state == SLE_ACB_STATE_DISCONNECTED) {
        g_sle_conn_hdl = 0;
        sle_start_announce(SLE_ADV_HANDLE_DEFAULT);
    }
}

void sle_server_pair_complete_cbk(uint16_t conn_id, const sle_addr_t *addr, errcode_t status)
{
    osal_printk("[uuid server] pair complete conn_id:%02x, status:%x\r\n",
        conn_id, status);
    osal_printk("[uuid server] pair complete addr:%02x:**:**:**:%02x:%02x\r\n",
        addr->addr[BT_INDEX_0], addr->addr[BT_INDEX_4], addr->addr[BT_INDEX_5]);
}

void sle_server_connect_param_update_req_cbk(uint16_t conn_id, errcode_t status, const sle_connection_param_update_req_t *param)
{
    osal_printk("[sle_server_connect_param_update_req_cbk] conn_id:%02x, status:%x, interval_min:%02x, interval_max:%02x, max_latency:%02x, supervision_timeout:%02x\r\n",
           conn_id, status, param->interval_min, param->interval_max, param->max_latency, param->supervision_timeout);
}

void sle_server_connect_param_update_cbk(uint16_t conn_id, errcode_t status,
    const sle_connection_param_update_evt_t *param)
{
    osal_printk("[sle_server_connect_param_update_cbk] conn_id:%02x, status:%x, interval:%02x, latency:%02x, supervision:%02x\r\n",
           conn_id, status, param->interval, param->latency, param->supervision);
}

void sle_server_auth_complete_cbk(uint16_t conn_id, const sle_addr_type_t *addr, errcode_t status, const sle_auth_info_evt_t *evt)
{
    unused(addr);
    osal_printk("[sle_server_auth_complete_cbk] auth complete conn_id:%02x, status:%x\r\n",
           conn_id, status);
    osal_printk("[sle_server_auth_complete_cbk] auth complete link_key:%02x, crypto_algo:%x, key_deriv_algo:%x, integr_chk_ind:%x\r\n",
           evt->link_key, evt->crypto_algo, evt->key_deriv_algo, evt->integr_chk_ind);
}

void sle_server_read_rssi_cbk(uint16_t conn_id, int8_t rssi, errcode_t status)
{
    printf("%s [sle_server_read_rssi_cbk] conn_id:%02x, rssi:%d, status:%x\r\n",
           conn_id, rssi, status);
}

static void sle_conn_register_cbks(void)
{
    sle_connection_callbacks_t conn_cbks = {0};
    conn_cbks.connect_state_changed_cb = sle_server_connect_state_changed_cbk;
    conn_cbks.pair_complete_cb = sle_server_pair_complete_cbk;
    sle_connection_register_callbacks(&conn_cbks);
}

/* 初始化uuid server */
errcode_t sle_uuid_server_init(void)
{
    enable_sle();
    sle_conn_register_cbks();
    sle_ssaps_register_cbks();
    sle_uuid_server_add();
    sle_uuid_server_adv_init();
    osal_printk("[uuid server] init ok\r\n");
    return ERRCODE_SLE_SUCCESS;

    
}

//****************hybrid******************//
errcode_t sle_hybridS_init(void)
{
    errcode_t ret = 0;
    ret = sle_ssaps_register_cbks();
    if(ret != ERRCODE_SUCC)
    {
        return ret;
    }
    osal_printk("[sle_hybrid] : sle_hybridS_init SUCC\r\n");
    return ERRCODE_SUCC;
}

void sle_server_sle_enable_cbk(errcode_t status)
{
    unused(status);

    errcode_t ret;

    ret = sle_uuid_server_add();
    if(ret != ERRCODE_SUCC)
    {
        osal_printk("[sle_enable_cbk]: sle_uuid_server_add FAIL\r\n");
    }

    ret = sle_uuid_server_adv_init();
    if(ret != ERRCODE_SUCC)
    {
        osal_printk("[sle_enable_cbk]: sle_uuid_server_adv_init FAIL\r\n");        
    }

}

void sle_server_sle_disable_cbk(errcode_t status)
{
    unused(status);
    osal_printk("[sle_disable_cbk]:server\r\n");
}

void sle_hybrids_wait_client_connected(void)
{
    while (g_sle_hybrids_conn_state != SLE_ACB_STATE_CONNECTED)
    {
        osal_msleep(100); 
    }
}

int sle_hybrids_send_data(uint8_t *data, uint8_t length)
{
    int ret;
    ret = sle_uuid_server_send_report_by_handle(data, length);
    return ret;
}
//---------------------------------------------------//
// #define SLE_UUID_SERVER_TASK_PRIO 26
// #define SLE_UUID_SERVER_STACK_SIZE 0x2000

// static void sle_uuid_server_entry(void)
// {
//     osal_task *task_handle = NULL;
//     osal_kthread_lock();
//     task_handle= osal_kthread_create((osal_kthread_handler)sle_uuid_server_init, 0, "sle_uuid_server",
//         SLE_UUID_SERVER_STACK_SIZE);
//     if (task_handle != NULL) {
//         osal_kthread_set_priority(task_handle, SLE_UUID_SERVER_TASK_PRIO);
//         osal_kfree(task_handle);
//     }
//     osal_kthread_unlock();
// }

// /* Run the app entry. */
// app_run(sle_uuid_server_entry);