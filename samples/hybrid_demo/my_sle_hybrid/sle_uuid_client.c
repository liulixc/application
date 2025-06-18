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
#define THIS_FILE_ID BTH_GLE_SAMPLE_UUID_CLIENT//这是干嘛的，没看懂一点ty

#define SLE_MTU_SIZE_DEFAULT        512
#define SLE_SEEK_INTERVAL_DEFAULT   100
#define SLE_SEEK_WINDOW_DEFAULT     100
#define UUID_16BIT_LEN 2
#define UUID_128BIT_LEN 16

#define SLE_TASK_DELAY_MS 1000        
#define SLE_CLIENT_LOG "[sle client]" 
static char g_sle_uuid_app_uuid[] = {0x39, 0xBE, 0xA8, 0x80, 0xFC, 0x70, 0x11, 0xEA,
                                     0xB7, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static sle_conn_and_service_t g_conn_and_service_arr[128] = {0};

uint8_t g_num_conn = 0;

static ssapc_callbacks_t g_sle_ssapc_cbk = {0};

sle_addr_t g_sle_remote_server_addr[128] = {0};

uint8_t g_num_remote_server_addr = 0;

ssapc_write_param_t g_sle_send_param = {0};

uint8_t g_client_id = 0;

char g_sle_server_name[128] = "";



sle_addr_t *sle_get_remote_server_addr(void)
{
    return &g_sle_remote_server_addr;
}

uint8_t sle_get_num_remote_server_addr(void)
{
    return g_num_remote_server_addr;
}

void sle_set_server_name(char *name)
{
    memcpy_s(g_sle_server_name, strlen(name), name, strlen(name));
}

sle_conn_and_service_t *sle_get_conn_and_service(void)
{
    return g_conn_and_service_arr;
}

void sle_start_scan()
{
    sle_seek_param_t param = {0};
    param.own_addr_type = SLE_ADDRESS_TYPE_PUBLIC;
    param.filter_duplicates = 0;
    param.seek_filter_policy = SLE_SEEK_FILTER_ALLOW_ALL;
    param.seek_phys = SLE_SEEK_PHY_1M;
    param.seek_type[0] = SLE_SEEK_ACTIVE;
    param.seek_interval[0] = SLE_SEEK_INTERVAL_DEFAULT;
    param.seek_window[0] = SLE_SEEK_WINDOW_DEFAULT;
    sle_set_seek_param(&param);
    sle_start_seek();
}

void sle_client_sle_enable_cbk(errcode_t status)
{
    if (status == 0) {
        sle_start_scan();
    }
}

void sle_client_sle_disable_cbk(errcode_t status)
{
    printf("%s [sle_client_sle_disable_cbk]\r\n", SLE_CLIENT_LOG);
}

void sle_client_sample_seek_enable_cbk(errcode_t status)
{
    if (status == 0) {
        return;
    }
}

void sle_client_sample_seek_disable_cbk(errcode_t status)
{

}

void sle_client_sample_seek_result_info_cbk(sle_seek_result_info_t *seek_result_data)
{
    if (seek_result_data == NULL)
    {
        osal_printk("%s [sle_client_seek_result_cbk] seek_result_data is NULL\r\n", SLE_CLIENT_LOG);
        return;
    }

    if (strstr((const char *)seek_result_data->data, g_sle_server_name) != NULL)
    {
        
        osal_printk("%s [sle_client_seek_result_cbk] target found, addr: %02x:%02x:%02x:%02x:%02x:%02x\r\n",
                    SLE_CLIENT_LOG,
                    seek_result_data->addr.addr[0], seek_result_data->addr.addr[1],
                    seek_result_data->addr.addr[2], seek_result_data->addr.addr[3],
                    seek_result_data->addr.addr[4], seek_result_data->addr.addr[5]);

        memcpy_s(&g_sle_remote_server_addr[g_num_remote_server_addr], sizeof(sle_addr_t),
                 &seek_result_data->addr, sizeof(sle_addr_t));
        g_num_remote_server_addr++; 

        sle_connect_remote_device(&seek_result_data->addr);
    }
    
    
    osal_printk("seek_result_info_cbk %s\r\n", seek_result_data->data);
    
}

// void sle_sample_seek_cbk_register(void)
// {
//     g_seek_cbk.sle_enable_cb = sle_sample_sle_enable_cbk;
//     g_seek_cbk.seek_enable_cb = sle_client_sample_seek_enable_cbk;
//     g_seek_cbk.seek_disable_cb = sle_client_sample_seek_disable_cbk;
//     g_seek_cbk.seek_result_cb = sle_client_sample_seek_result_info_cbk;
// }

void sle_client_connect_state_changed_cbk(uint16_t conn_id, const sle_addr_t *addr,
    sle_acb_state_t conn_state, sle_pair_state_t pair_state, sle_disc_reason_t disc_reason)
{
    osal_printk("[ssap client] conn state changed conn_id:%d, addr:%02x***%02x%02x\n", conn_id, addr->addr[0],
        addr->addr[4], addr->addr[5]); /* 0 4 5: addr index */
    
    if (conn_state == SLE_ACB_STATE_CONNECTED) 
    {
        osal_printk("[sle_client_connect_state_changed_cbk] SLE_ACB_STATE_CONNECTED\r\n");
        g_num_conn++;
        ssap_exchange_info_t info = {0};
        info.mtu_size = SLE_MTU_SIZE_DEFAULT;
        info.version = 1;
        ssapc_exchange_info_req(0, conn_id, &info);

    }
    else if(conn_state == SLE_ACB_STATE_NONE)
    {
        osal_printk("[sle_client_connect_state_changed_cbk] SLE_ACB_STATE_NONE\r\n");
    }
    else if(conn_state == SLE_ACB_STATE_DISCONNECTED)
    {
         for (int i = 0; i < g_num_conn; i++)
        {
            if (g_conn_and_service_arr[i].conn_id == conn_id) 
            {
                
                int j;
                for (j = i; j < g_num_conn - 1; j++)
                {
                    g_conn_and_service_arr[j] = g_conn_and_service_arr[j + 1];
                }
                
                memset(&g_conn_and_service_arr[j], 0, sizeof(sle_conn_and_service_t));
                
                g_num_conn--;
            }
        }

        for (int i = 0; i < g_num_remote_server_addr; i++)
        {
            if (memcmp(&g_sle_remote_server_addr[i], addr, sizeof(sle_addr_t)) == 0) 
            {
                
                int j;
                for (j = i; j < g_num_remote_server_addr - 1; j++)
                {
                    g_sle_remote_server_addr[j] = g_sle_remote_server_addr[j + 1];
                }
                
                memset(&g_sle_remote_server_addr[j], 0, sizeof(sle_addr_t));
                
                g_num_remote_server_addr--;
            }
        }
        osal_printk("sle disconnected,disreason : %d\r\n",disc_reason);
    }
}

// void sle_client_pair_complete_cbk(uint16_t conn_id, const sle_addr_t *addr, errcode_t status)
// {
//     osal_printk("[ssap client] pair complete conn_id:%d, addr:%02x***%02x%02x\n", conn_id, addr->addr[0],
//         addr->addr[4], addr->addr[5]); /* 0 4 5: addr index */
//     if (status == 0) {
//         ssap_exchange_info_t info = {0};
//         info.mtu_size = SLE_MTU_SIZE_DEFAULT;
//         info.version = 1;
//         ssapc_exchange_info_req(1, g_conn_id, &info);
//     }
// }

// void sle_sample_connect_cbk_register(void)
// {
//     g_connect_cbk.connect_state_changed_cb = sle_client_connect_state_changed_cbk;
//     g_connect_cbk.pair_complete_cb = sle_client_pair_complete_cbk;
// }

void sle_client_exchange_info_cbk(uint8_t client_id, uint16_t conn_id, ssap_exchange_info_t *param,
    errcode_t status)
{
    osal_printk("[ssap client] pair complete client id:%d status:%d\n", client_id, status);
    osal_printk("[ssap client] exchange mtu, mtu size: %d, version: %d.\n",
        param->mtu_size, param->version);
    if (status == ERRCODE_SUCC)
    {
        ssapc_find_structure_param_t find_param = {0};
        find_param.type = SSAP_FIND_TYPE_PRIMARY_SERVICE;
        find_param.start_hdl = 1;
        find_param.end_hdl = 0xFFFF;
        ssapc_find_structure(client_id, conn_id, &find_param);
    }
}

void sle_client_find_structure_cbk(uint8_t client_id, uint16_t conn_id, ssapc_find_service_result_t *service,
    errcode_t status)
{
    osal_printk("[ssap client] find structure cbk client: %d conn_id:%d status: %d \n",
        client_id, conn_id, status);
    osal_printk("[ssap client] find structure start_hdl:[0x%02x], end_hdl:[0x%02x], uuid len:%d\r\n",
        service->start_hdl, service->end_hdl, service->uuid.len);
    
    if (status == ERRCODE_SUCC)
    {
        
        for (int i = 0; i < g_num_conn; i++)
        {
            if (g_conn_and_service_arr[i].conn_id == conn_id) 
            {
                
                g_conn_and_service_arr[i].find_service_result.start_hdl = service->start_hdl;
                
                g_conn_and_service_arr[i].find_service_result.end_hdl = service->end_hdl;
                
                memcpy_s(&g_conn_and_service_arr[i].find_service_result.uuid, sizeof(sle_uuid_t),
                         &service->uuid, sizeof(sle_uuid_t));
            }
        }
    }
}

static void sle_client_find_structure_cmp_cbk(uint8_t client_id, uint16_t conn_id,
                                              ssapc_find_structure_result_t *structure_result,
                                              errcode_t status)
{
    unused(conn_id);

    osal_printk("%s [sle_client_find_structure_cmp_cbk] client id:%d status:%d type:%d uuid len:%d \r\n",
                SLE_CLIENT_LOG, client_id, status, structure_result->type, structure_result->uuid.len);
}

static void sle_client_find_property_cbk(uint8_t client_id, uint16_t conn_id,
                                         ssapc_find_property_result_t *property, errcode_t status)
{
    osal_printk("%s [sle_client_find_property_cbk] client id: %d, conn id: %d, operate ind: %d, "
                "descriptors count: %d status:%d property->handle %d\r\n",
                SLE_CLIENT_LOG,
                client_id, conn_id, property->operate_indication,
                property->descriptors_count, status, property->handle);
    g_sle_send_param.handle = property->handle;
    g_sle_send_param.type = SSAP_PROPERTY_TYPE_VALUE; 
}

static void sle_client_write_cfm_cbk(uint8_t client_id, uint16_t conn_id,
                                     ssapc_write_result_t *write_result, errcode_t status)
{
    
}

static void sle_client_read_cfm_cbk(uint8_t client_id, uint16_t conn_id,
                                    ssapc_handle_value_t *read_data, errcode_t status)
{
    osal_printk("[sle_client_read_cfm_cbk] client id:0x%x conn id:0x%x status:0x%x\r\n", client_id, conn_id, status);
    osal_printk("[sle_client_read_cfm_cbk] handle:0x%x, type:0x%x, len:0x%x\r\n", read_data->handle, read_data->type,
                read_data->data_len);
    for (uint16_t idx = 0; idx < read_data->data_len; idx++)
    {
        osal_printk("[sle_client_read_cfm_cbk] [0x%x] 0x%02x\r\n", idx, read_data->data[idx]);
    }
}

void ssapc_notification_cbk(uint8_t client_id, uint16_t conn_id, ssapc_handle_value_t *data,
                            errcode_t status)
{
    (void)client_id;
    (void)conn_id;
    (void)status;

    data->data[data->data_len - 1] = '\0';
    printf("[ssapc_notification_cbk] server_send_data: %s\r\n", data->data);
}

void ssapc_indication_cbk(uint8_t client_id, uint16_t conn_id, ssapc_handle_value_t *data,
                          errcode_t status)
{
    (void)client_id;
    (void)conn_id;
    (void)status;

    data->data[data->data_len - 1] = '\0';
    printf("[ssapc_indication_cbk] server_send_data: %s\r\n", data->data);
}

static void sle_client_ssapc_cbk_register(ssapc_notification_callback ssapc_notification_cbk,
                                          ssapc_indication_callback ssapc_indication_cbk)
{
    g_sle_ssapc_cbk.exchange_info_cb = sle_client_exchange_info_cbk;           
    g_sle_ssapc_cbk.find_structure_cb = sle_client_find_structure_cbk;         
    g_sle_ssapc_cbk.ssapc_find_property_cbk = sle_client_find_property_cbk;    
    g_sle_ssapc_cbk.find_structure_cmp_cb = sle_client_find_structure_cmp_cbk; 
    g_sle_ssapc_cbk.write_cfm_cb = sle_client_write_cfm_cbk;                   
    g_sle_ssapc_cbk.read_cfm_cb = sle_client_read_cfm_cbk;                     
    g_sle_ssapc_cbk.notification_cb = ssapc_notification_cbk;                  
    g_sle_ssapc_cbk.indication_cb = ssapc_indication_cbk;                      
    ssapc_register_callbacks(&g_sle_ssapc_cbk);                                
}

static errcode_t sle_uuid_client_register(void)
{
    errcode_t ret;

    sle_uuid_t app_uuid = {0};
    printf("[uuid client] ssapc_register_client \r\n");
    app_uuid.len = sizeof(g_sle_uuid_app_uuid);
    if (memcpy_s(app_uuid.uuid, app_uuid.len, g_sle_uuid_app_uuid, sizeof(g_sle_uuid_app_uuid)) != EOK)
    {
        return ERRCODE_FAIL;
    }

    ret = ssapc_register_client(&app_uuid, &g_client_id);

    return ret;
}



// void sle_client_init()
// {
//     sle_sample_seek_cbk_register();
//     sle_sample_connect_cbk_register();
//     sle_sample_ssapc_cbk_register();
//     sle_announce_seek_register_callbacks(&g_seek_cbk);
//     sle_connection_register_callbacks(&g_connect_cbk);
//     ssapc_register_callbacks(&g_ssapc_cbk);
// }



//*************hybrid******************//
errcode_t sle_client_send_report_by_handle(const uint8_t *data, uint8_t len)
{
    
    ssapc_write_param_t param = {0};  

    int ret = 0;
    for (int i = 0; i < g_num_conn; i++)
    {
        
        if (g_conn_and_service_arr[i].find_service_result.start_hdl == 0)
        {
            continue; 
        }

        param.handle = g_conn_and_service_arr[i].find_service_result.start_hdl;

        param.type = SSAP_PROPERTY_TYPE_VALUE;

        param.data_len = len + 1; 

        param.data = osal_vmalloc(param.data_len);
        if (param.data == NULL)
        {
            printf("[sle_client_send_report_by_handle] osal_vmalloc fail\r\n");
            return ERRCODE_FAIL;
        }
        if (memcpy_s(param.data, param.data_len, data, len) != EOK)
        {
            osal_vfree(param.data);
            return ERRCODE_FAIL;
        }

        ret = SsapWriteReq(g_client_id, g_conn_and_service_arr[i].conn_id, &param);
        if (ret != ERRCODE_SUCC)
        {
            printf("SsapWriteReq error:%d connid:%d\r\n", ret, g_conn_and_service_arr[i].conn_id);
        }

        osal_vfree(param.data);
    }

    return ERRCODE_SUCC;
}

int sle_hybridc_send_data(uint8_t *data, uint8_t length)
{
    int ret;
    ret = sle_client_send_report_by_handle(data, length); 
    return ret;
}

void sle_hybridc_init()
{
    
    uint32_t ret = sle_uuid_client_register();
    printf("sle_uuid_client_register_errcode:%d\r\n", ret);

    sle_client_ssapc_cbk_register(ssapc_notification_cbk, ssapc_indication_cbk);
}

void sle_set_hybridc_addr(void)
{
    
    uint8_t local_addr[SLE_ADDR_LEN] = {0x13, 0x67, 0x5c, 0x07, 0x00, 0x51};                                            
    printf("Hybrid-C local_addr: %02x:%02x:%02x:%02x:%02x:%02x\r\n",
           local_addr[0], local_addr[1], local_addr[2], local_addr[3], local_addr[4], local_addr[5]);
    sle_addr_t local_address;

    local_address.type = SLE_ADDRESS_TYPE_PUBLIC;

    (void)memcpy_s(local_address.addr, SLE_ADDR_LEN, local_addr, SLE_ADDR_LEN);

    sle_set_local_addr(&local_address);
}
