/**
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2022. All rights reserved.
 *
 * Description: SLE private service register sample of client.
 */
#include "securec.h"
#include "soc_osal.h"
#include "app_init.h"
#include "common_def.h"
#include "sle_device_discovery.h"
#include "sle_connection_manager.h"
#include "sle_ssap_client.h"
#include "sle_uuid_client.h"

#undef THIS_FILE_ID
#define THIS_FILE_ID BTH_GLE_SAMPLE_UUID_CLIENT

#define SLE_MTU_SIZE_DEFAULT        300
#define SLE_SEEK_INTERVAL_DEFAULT   100
#define SLE_SEEK_WINDOW_DEFAULT     100
#define UUID_16BIT_LEN 2
#define UUID_128BIT_LEN 16

sle_announce_seek_callbacks_t g_seek_cbk = {0};
sle_connection_callbacks_t    g_connect_cbk = {0};
ssapc_callbacks_t             g_ssapc_cbk = {0};
sle_addr_t                    g_remote_addr = {0};
uint16_t                      g_conn_id = 0;
ssapc_find_service_result_t   g_find_service_result = {0};
char g_sle_server_name[128] = "";
uint8_t g_sle_service_found = 0;


sle_acb_state_t g_sle_hybridc_conn_state = SLE_ACB_STATE_NONE;

void sle_set_remote_server_name(char *name)
{
    memcpy_s(g_sle_server_name,strlen(name),name,strlen(name));
}

sle_addr_t *sle_get_remote_server_addr(void)
{
    return &g_remote_addr;
}

void sle_sample_sle_enable_cbk(errcode_t status)
{
    if (status == 0) {
        sle_start_scan();
    }
}

void sle_client_sample_seek_enable_cbk(errcode_t status)
{
    if (status == 0) {
        return;
    }
}

void sle_client_sample_seek_disable_cbk(errcode_t status)
{
    if (status == 0) {
        sle_connect_remote_device(&g_remote_addr);
    }
}

void sle_client_sample_seek_result_info_cbk(sle_seek_result_info_t *seek_result_data)
{
    if (seek_result_data != NULL) {
        (void)memcpy_s(&g_remote_addr, sizeof(sle_addr_t), &seek_result_data->addr, sizeof(sle_addr_t));
        sle_stop_seek();
    }
}

void sle_sample_seek_cbk_register(void)
{
    g_seek_cbk.sle_enable_cb = sle_sample_sle_enable_cbk;
    g_seek_cbk.seek_enable_cb = sle_client_sample_seek_enable_cbk;
    g_seek_cbk.seek_disable_cb = sle_client_sample_seek_disable_cbk;
    g_seek_cbk.seek_result_cb = sle_client_sample_seek_result_info_cbk;
}

void sle_client_connect_state_changed_cbk(uint16_t conn_id, const sle_addr_t *addr,
    sle_acb_state_t conn_state, sle_pair_state_t pair_state, sle_disc_reason_t disc_reason)
{
    g_sle_hybridc_conn_state = conn_state; 
    osal_printk("[ssap client] conn state changed conn_id:%d, addr:%02x***%02x%02x\n", conn_id, addr->addr[0],
        addr->addr[4], addr->addr[5]); /* 0 4 5: addr index */
    
    if (conn_state == SLE_ACB_STATE_CONNECTED) 
    {
        osal_printk("[sle_client_connect_state_changed_cbk] SLE_ACB_STATE_CONNECTED\r\n");
        g_conn_id = conn_id;
        if (pair_state == SLE_PAIR_NONE) 
        {
            sle_pair_remote_device(&g_remote_addr);
        }
    }
    else if(conn_state == SLE_ACB_STATE_NONE)
    {
        osal_printk("[sle_client_connect_state_changed_cbk] SLE_ACB_STATE_NONE\r\n");

        g_sle_service_found = 0;
    }
    else if(conn_state == SLE_ACB_STATE_DISCONNECTED)
    {
        sle_remove_paired_remote_device(addr);
        sle_start_scan();
        osal_printk("sle disconnected,disreason : %d\r\n",disc_reason);
    }
}

void sle_client_pair_complete_cbk(uint16_t conn_id, const sle_addr_t *addr, errcode_t status)
{
    osal_printk("[ssap client] pair complete conn_id:%d, addr:%02x***%02x%02x\n", conn_id, addr->addr[0],
        addr->addr[4], addr->addr[5]); /* 0 4 5: addr index */
    if (status == 0) {
        ssap_exchange_info_t info = {0};
        info.mtu_size = SLE_MTU_SIZE_DEFAULT;
        info.version = 1;
        ssapc_exchange_info_req(1, g_conn_id, &info);
    }
}

void sle_sample_connect_cbk_register(void)
{
    g_connect_cbk.connect_state_changed_cb = sle_client_connect_state_changed_cbk;
    g_connect_cbk.pair_complete_cb = sle_client_pair_complete_cbk;
}

void sle_sample_exchange_info_cbk(uint8_t client_id, uint16_t conn_id, ssap_exchange_info_t *param,
    errcode_t status)
{
    osal_printk("[ssap client] pair complete client id:%d status:%d\n", client_id, status);
    osal_printk("[ssap client] exchange mtu, mtu size: %d, version: %d.\n",
        param->mtu_size, param->version);

    ssapc_find_structure_param_t find_param = {0};
    find_param.type = SSAP_FIND_TYPE_PRIMARY_SERVICE;
    find_param.start_hdl = 1;
    find_param.end_hdl = 0xFFFF;
    ssapc_find_structure(0, conn_id, &find_param);
}

void sle_sample_find_structure_cbk(uint8_t client_id, uint16_t conn_id, ssapc_find_service_result_t *service,
    errcode_t status)
{
    osal_printk("[ssap client] find structure cbk client: %d conn_id:%d status: %d \n",
        client_id, conn_id, status);
    osal_printk("[ssap client] find structure start_hdl:[0x%02x], end_hdl:[0x%02x], uuid len:%d\r\n",
        service->start_hdl, service->end_hdl, service->uuid.len);
    if (service->uuid.len == UUID_16BIT_LEN) {
        osal_printk("[ssap client] structure uuid:[0x%02x][0x%02x]\r\n",
            service->uuid.uuid[14], service->uuid.uuid[15]); /* 14 15: uuid index */
    } else {
        for (uint8_t idx = 0; idx < UUID_128BIT_LEN; idx++) {
            osal_printk("[ssap client] structure uuid[%d]:[0x%02x]\r\n", idx, service->uuid.uuid[idx]);
        }
    }
    g_find_service_result.start_hdl = service->start_hdl;
    g_find_service_result.end_hdl = service->end_hdl;
    g_sle_service_found = 1;
    memcpy_s(&g_find_service_result.uuid, sizeof(sle_uuid_t), &service->uuid, sizeof(sle_uuid_t));
}

void sle_sample_find_structure_cmp_cbk(uint8_t client_id, uint16_t conn_id,
    ssapc_find_structure_result_t *structure_result, errcode_t status)
{
    osal_printk("[ssap client] find structure cmp cbk client id:%d status:%d type:%d uuid len:%d \r\n",
        client_id, status, structure_result->type, structure_result->uuid.len);
    if (structure_result->uuid.len == UUID_16BIT_LEN) {
        osal_printk("[ssap client] find structure cmp cbk structure uuid:[0x%02x][0x%02x]\r\n",
            structure_result->uuid.uuid[14], structure_result->uuid.uuid[15]); /* 14 15: uuid index */
    } else {
        for (uint8_t idx = 0; idx < UUID_128BIT_LEN; idx++) {
            osal_printk("[ssap client] find structure cmp cbk structure uuid[%d]:[0x%02x]\r\n", idx,
                structure_result->uuid.uuid[idx]);
        }
    }
}

void sle_sample_find_property_cbk(uint8_t client_id, uint16_t conn_id,
    ssapc_find_property_result_t *property, errcode_t status)
{
    osal_printk("[ssap client] find property cbk, client id: %d, conn id: %d, operate ind: %d, "
        "descriptors count: %d status:%d.\n", client_id, conn_id, property->operate_indication,
        property->descriptors_count, status);
    for (uint16_t idx = 0; idx < property->descriptors_count; idx++) {
        osal_printk("[ssap client] find property cbk, descriptors type [%d]: 0x%02x.\n",
            idx, property->descriptors_type[idx]);
    }
    if (property->uuid.len == UUID_16BIT_LEN) {
        osal_printk("[ssap client] find property cbk, uuid: %02x %02x.\n",
            property->uuid.uuid[14], property->uuid.uuid[15]); /* 14 15: uuid index */
    } else if (property->uuid.len == UUID_128BIT_LEN) {
        for (uint16_t idx = 0; idx < UUID_128BIT_LEN; idx++) {
            osal_printk("[ssap client] find property cbk, uuid [%d]: %02x.\n",
                idx, property->uuid.uuid[idx]);
        }
    }
}

void sle_sample_write_cfm_cbk(uint8_t client_id, uint16_t conn_id, ssapc_write_result_t *write_result,
    errcode_t status)
{   
    osal_printk("[ssap client] write cfm cbk, client id: %d status:%d.\n", client_id, status);
    ssapc_read_req(0, conn_id, write_result->handle, write_result->type);
}

void sle_sample_read_cfm_cbk(uint8_t client_id, uint16_t conn_id, ssapc_handle_value_t *read_data,
    errcode_t status)
{
    osal_printk("[ssap client] read cfm cbk client id: %d conn id: %d status: %d\n",
        client_id, conn_id, status);
    osal_printk("[ssap client] read cfm cbk handle: %d, type: %d , len: %d\n",
        read_data->handle, read_data->type, read_data->data_len);
    for (uint16_t idx = 0; idx < read_data->data_len; idx++) {
        osal_printk("[ssap client] read cfm cbk[%d] 0x%02x\r\n", idx, read_data->data[idx]);
    }
}

void sle_sample_notification_cbk(uint8_t client_id, uint16_t conn_id, ssapc_handle_value_t *data,
    errcode_t status)
{
    unused(client_id);
    unused(conn_id);
    unused(status);
    osal_printk("[client]recv:data_len:%d\r\n",data->data_len);
    osal_printk("[client]recv data : %s\r\n",data->data);
}

void sle_sample_ssapc_cbk_register(void)
{
    g_ssapc_cbk.exchange_info_cb = sle_sample_exchange_info_cbk;
    g_ssapc_cbk.find_structure_cb = sle_sample_find_structure_cbk;
    g_ssapc_cbk.find_structure_cmp_cb = sle_sample_find_structure_cmp_cbk;
    g_ssapc_cbk.ssapc_find_property_cbk = sle_sample_find_property_cbk;
    g_ssapc_cbk.write_cfm_cb = sle_sample_write_cfm_cbk;
    g_ssapc_cbk.read_cfm_cb = sle_sample_read_cfm_cbk;
    g_ssapc_cbk.notification_cb = sle_sample_notification_cbk;
    //g_ssapc_cbk.indication_cb = sle_sample_indication_cbk;
}

void sle_client_init()
{
    sle_sample_seek_cbk_register();
    sle_sample_connect_cbk_register();
    sle_sample_ssapc_cbk_register();
    sle_announce_seek_register_callbacks(&g_seek_cbk);
    sle_connection_register_callbacks(&g_connect_cbk);
    ssapc_register_callbacks(&g_ssapc_cbk);
}

void sle_start_scan()
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

//*************hybrid******************//
errcode_t sle_client_send_report_by_handle(const uint8_t *data, uint8_t len)
{
    
    ssapc_write_param_t param = {0};  

    param.handle = g_find_service_result.start_hdl;

    param.type = SSAP_PROPERTY_TYPE_VALUE;

    param.data_len = len + 1; 

    param.data = osal_vmalloc(param.data_len);
    if (param.data == NULL)
    {
        osal_printk("[sle_client_send_report_by_handle] osal_vmalloc fail\r\n");
        return ERRCODE_SUCC;
    }
    if (memcpy_s(param.data, param.data_len, data, len) != EOK)
    {
        osal_vfree(param.data);
        return ERRCODE_SUCC;
    }

    if(ssapc_write_req(0, g_conn_id, &param) != ERRCODE_SUCC)
    {
        osal_printk("[sle_client_send_report_by_handle] SsapWriteReq fail\r\n");
        osal_vfree(param.data);
        return ERRCODE_SUCC;
    }

    osal_vfree(param.data);
    return ERRCODE_SUCC;
}

errcode_t sle_hybridC_init(void)
{
    errcode_t ret = 0;
    sle_sample_ssapc_cbk_register();
    ret = ssapc_register_callbacks(&g_ssapc_cbk);
    if(ret != ERRCODE_SUCC)
    {
        osal_printk("[sle_hybridC] : sle client init fali\r\n");
        return ret;
    }
    osal_printk("[sle_hybridC] : sle client init succ\r\n");
    return ret;
}

void sle_client_sle_enable_cbk(errcode_t status)
{
    if (status != ERRCODE_SUCC)
    {
        osal_printk("[sle_client_sle_enable_cbk] status error\r\n");
    }
    else
    {
        osal_printk("[sle_client_sle_enable_cbk] status success\r\n");
        osal_msleep(1000);
        sle_start_scan();
    }
}

void sle_client_sle_disable_cbk(errcode_t status)
{
    unused(status);
    osal_printk("[sle_disable_cbk]:client\r\n");
}

void sle_set_hybridc_addr(void)
{
    uint8_t local_addr[SLE_ADDR_LEN] = {0x13, 0x67, 0x5c, 0x07, 0x00, 0x51}; 

    osal_printk("Hybrid-C local_addr: %02x:%02x:%02x:%02x:%02x:%02x\r\n",
           local_addr[0], local_addr[1], local_addr[2], local_addr[3], local_addr[4], local_addr[5]);
    sle_addr_t local_address;
    local_address.type = SLE_ADDRESS_TYPE_PUBLIC;
    memcpy_s(local_address.addr,SLE_ADDR_LEN,local_addr,SLE_ADDR_LEN);
    sle_set_local_addr(&local_address);
}

void sle_hybridc_wait_service_found(void)
{
    while (g_sle_service_found == 0)
    {
        osal_msleep(100); 
    }
}

int sle_hybridc_send_data(uint8_t *data, uint8_t length)
{
    int ret;
    ret = sle_client_send_report_by_handle(data, length); 
    return ret;
}
//------------------hybrid-----------------//
// #define SLE_UUID_CLIENT_TASK_PRIO 26
// #define SLE_UUID_CLIENT_STACK_SIZE 0x2000

// static void sle_uuid_client_entry(void)
// {
//     osal_task *task_handle = NULL;
//     osal_kthread_lock();
//     task_handle= osal_kthread_create((osal_kthread_handler)sle_client_init, 0, "sle_gatt_client",
//         SLE_UUID_CLIENT_STACK_SIZE);
//     if (task_handle != NULL) {
//         osal_kthread_set_priority(task_handle, SLE_UUID_CLIENT_TASK_PRIO);
//         osal_kfree(task_handle);
//     }
//     osal_kthread_unlock();
// }

// /* Run the app entry. */
// app_run(sle_uuid_client_entry);