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

#define SLE_MTU_SIZE_DEFAULT 512      
#define SLE_SEEK_INTERVAL_DEFAULT 100 
#define SLE_SEEK_WINDOW_DEFAULT 100   
#define UUID_16BIT_LEN 2              
#define UUID_128BIT_LEN 16            

#define SLE_TASK_DELAY_MS 1000        
#define SLE_CLIENT_LOG "[sle client]" 

static char g_sle_uuid_app_uuid[] = {0x39, 0xBE, 0xA8, 0x80, 0xFC, 0x70, 0x11, 0xEA,
                                     0xB7, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static ssapc_find_service_result_t g_sle_find_service_result = {0};

static sle_announce_seek_callbacks_t g_sle_seek_cbk = {0};

static sle_connection_callbacks_t g_sle_connect_cbk = {0};

static ssapc_callbacks_t g_sle_ssapc_cbk = {0};

static sle_addr_t g_sle_remote_addr = {0};

ssapc_write_param_t g_sle_send_param = {0};

uint8_t g_sle_connected = 0;

uint8_t g_sle_service_found = 0;

uint16_t g_sle_conn_id = 0;

uint8_t g_client_id = 0;

char g_sle_server_name[128] = "";

void sle_set_server_name(char *name)
{
    memcpy_s(g_sle_server_name, strlen(name), name, strlen(name));
}

uint8_t sle_is_connected(void)
{
    return g_sle_connected;
}

uint16_t sle_get_conn_id(void)
{
    return g_sle_conn_id;
}

ssapc_write_param_t *sle_get_send_param(void)
{
    return &g_sle_send_param;
}

void sle_start_scan(void)
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

static void sle_client_sle_enable_cbk(errcode_t status)
{
    if (status != ERRCODE_SUCC)
    {
        osal_printk("%s [sle_client_sle_enable_cbk] status error\r\n", SLE_CLIENT_LOG);
    }
    else
    {
        osal_msleep(SLE_TASK_DELAY_MS);

        sle_start_scan();
    }
}

static void sle_client_seek_enable_cbk(errcode_t status)
{
    if (status != ERRCODE_SUCC)
    {
        osal_printk("%s [sle_client_seek_enable_cbk] status error\r\n", SLE_CLIENT_LOG);
    }
}

static void sle_client_seek_result_info_cbk(sle_seek_result_info_t *seek_result_data)
{
    if (seek_result_data == NULL)
    {
        osal_printk("%s [sle_client_seek_result_info_cbk] seek_result_data is NULL\r\n", SLE_CLIENT_LOG);
        return;
    }

    if (strstr((const char *)seek_result_data->data, g_sle_server_name) != NULL)
    {
        
        osal_printk("%s [sle_client_seek_result_info_cbk] target found, addr: %02x:%02x:%02x:%02x:%02x:%02x\r\n",
                    SLE_CLIENT_LOG,
                    seek_result_data->addr.addr[0], seek_result_data->addr.addr[1],
                    seek_result_data->addr.addr[2], seek_result_data->addr.addr[3],
                    seek_result_data->addr.addr[4], seek_result_data->addr.addr[5]);

        memcpy_s(&g_sle_remote_addr, sizeof(sle_addr_t), &seek_result_data->addr, sizeof(sle_addr_t));

        SleStopSeek();
    }
}

static void sle_client_seek_disable_cbk(errcode_t status)
{
    if (status != ERRCODE_SUCC)
    {
        osal_printk("%s [sle_client_seek_disable_cbk] status error = %x\r\n", SLE_CLIENT_LOG, status);
    }
    else
    {
        SleConnectRemoteDevice(&g_sle_remote_addr); 
    }
}

static void sle_client_seek_cbk_register(void)
{
    g_sle_seek_cbk.sle_enable_cb = sle_client_sle_enable_cbk;        
    g_sle_seek_cbk.seek_enable_cb = sle_client_seek_enable_cbk;      
    g_sle_seek_cbk.seek_result_cb = sle_client_seek_result_info_cbk; 
    g_sle_seek_cbk.seek_disable_cb = sle_client_seek_disable_cbk;    
    sle_announce_seek_register_callbacks(&g_sle_seek_cbk);           
}

static void sle_client_connect_state_changed_cbk(uint16_t conn_id, const sle_addr_t *addr,
                                                 sle_acb_state_t conn_state, sle_pair_state_t pair_state, sle_disc_reason_t disc_reason)
{
    unused(addr);
    unused(pair_state);
    osal_printk("%s [sle_client_connect_state_changed_cbk] conn_id:0x%02x, conn_state:0x%x, pair_state:0x%x, \
        disc_reason:0x%x\r\n",
                SLE_CLIENT_LOG, conn_id, conn_state, pair_state, disc_reason);
    osal_printk("%s [sle_client_connect_state_changed_cbk] addr:%02x:%02x:%02x:%02x:%02x:%02x\r\n",
                SLE_CLIENT_LOG, addr->addr[0], addr->addr[1], addr->addr[2], addr->addr[3],
                addr->addr[4], addr->addr[5]);

    if (conn_state == SLE_ACB_STATE_CONNECTED)
    {
        osal_printk("%s [sle_client_connect_state_changed_cbk] SLE_ACB_STATE_CONNECTED\r\n", SLE_CLIENT_LOG);

        g_sle_conn_id = conn_id;

        g_sle_connected = 1;

        if (pair_state == SLE_PAIR_NONE)
        {
            SlePairRemoteDevice(addr);
        }
    }

    else if (conn_state == SLE_ACB_STATE_NONE)
    {
        osal_printk("%s [sle_client_connect_state_changed_cbk] SLE_ACB_STATE_NONE\r\n", SLE_CLIENT_LOG);
    }

    else if (conn_state == SLE_ACB_STATE_DISCONNECTED)
    {
        osal_printk("%s [sle_client_connect_state_changed_cbk] SLE_ACB_STATE_DISCONNECTED\r\n", SLE_CLIENT_LOG);
        sle_start_scan(); 
    }

    else
    {
        osal_printk("%s [sle_client_connect_state_changed_cbk] status error\r\n", SLE_CLIENT_LOG);
    }
}

static void sle_client_pair_complete_cbk(uint16_t conn_id, const sle_addr_t *addr, errcode_t status)
{
    osal_printk("%s [sle_pair_complete_cbk] pair complete conn_id:%02x, status:%x\r\n", SLE_CLIENT_LOG,
                conn_id, status);
    osal_printk("%s [sle_pair_complete_cbk] pair complete addr:%02x:%02x:%02x:%02x:%02x:%02x\r\n",
                SLE_CLIENT_LOG, addr->addr[0], addr->addr[1], addr->addr[2], addr->addr[3],
                addr->addr[4], addr->addr[5]);

    if (status == ERRCODE_SUCC) 
    {
        
        ssap_exchange_info_t info = {0};

        info.mtu_size = SLE_MTU_SIZE_DEFAULT;

        info.version = 1;

        SsapcExchangeInfoReq(1, conn_id, &info); 
    }
}

static void sle_client_connect_cbk_register(void)
{

    g_sle_connect_cbk.connect_state_changed_cb = sle_client_connect_state_changed_cbk; 
    g_sle_connect_cbk.connect_param_update_req_cb = NULL;                               
    g_sle_connect_cbk.connect_param_update_cb = NULL;                                  
    g_sle_connect_cbk.auth_complete_cb = NULL;                                        
    g_sle_connect_cbk.pair_complete_cb = sle_client_pair_complete_cbk;                
    g_sle_connect_cbk.read_rssi_cb = NULL;                                            

    SleConnectionRegisterCallbacks(&g_sle_connect_cbk); 
}

static void sle_client_exchange_info_cbk(uint8_t client_id, uint16_t conn_id, ssap_exchange_info_t *param,
                                         errcode_t status)
{
    osal_printk("%s [sle_client_exchange_info_cbk] client id:%d status:%d\r\n",
                SLE_CLIENT_LOG, client_id, status);
    osal_printk("%s [sle_client_exchange_info_cbk] mtu size: %d, version: %d\r\n",
                SLE_CLIENT_LOG, param->mtu_size, param->version);

    if (status == ERRCODE_SUCC)
    {
        ssapc_find_structure_param_t find_param = {0}; 

        find_param.type = SSAP_FIND_TYPE_PRIMARY_SERVICE;

        find_param.start_hdl = 1;

        find_param.end_hdl = 0xFFFF;

        ssapc_find_structure(client_id, conn_id, &find_param); 
    }
}

static void sle_client_find_structure_cbk(uint8_t client_id, uint16_t conn_id,
                                          ssapc_find_service_result_t *service,
                                          errcode_t status)
{
    osal_printk("%s [sle_client_find_structure_cbk] client: %d conn_id:%d status: %d \r\n",
                SLE_CLIENT_LOG, client_id, conn_id, status);
    osal_printk("%s [sle_client_find_structure_cbk] start_hdl:[0x%02x], end_hdl:[0x%02x], uuid len:%d\r\n",
                SLE_CLIENT_LOG, service->start_hdl, service->end_hdl, service->uuid.len);
    if (service->uuid.len == UUID_16BIT_LEN)
    {
        osal_printk("[sle_client_find_structure_cbk] structure uuid:[0x%02x][0x%02x]\r\n",
                    service->uuid.uuid[14], service->uuid.uuid[15]); 
    }
    else
    {
        for (uint8_t idx = 0; idx < UUID_128BIT_LEN; idx++)
        {
            osal_printk("[sle_client_find_structure_cbk] structure uuid[0x%x]:[0x%02x]\r\n",
                        idx, service->uuid.uuid[idx]);
        }
    }

    if (status == ERRCODE_SUCC)
    {
        
        g_sle_find_service_result.start_hdl = service->start_hdl;

        g_sle_find_service_result.end_hdl = service->end_hdl;

        memcpy_s(&g_sle_find_service_result.uuid, sizeof(sle_uuid_t), &service->uuid, sizeof(sle_uuid_t));

        g_sle_service_found = 1;
    }
}

static void sle_client_find_property_cbk(uint8_t client_id, uint16_t conn_id,
                                         ssapc_find_property_result_t *property, errcode_t status)
{
    osal_printk("%s [sle_client_find_property_cbk] client id: %d, conn id: %d, operate ind: %d, "
                "descriptors count: %d status:%d property->handle %d\r\n",
                SLE_CLIENT_LOG,
                client_id, conn_id, property->operate_indication,
                property->descriptors_count, status, property->handle);
    for (uint16_t idx = 0; idx < property->descriptors_count; idx++)
    {
        osal_printk("[sle_client_find_property_cbk] descriptors type [0x%x]: 0x%02x.\r\n", idx,
                    property->descriptors_type[idx]);
    }
    if (property->uuid.len == UUID_16BIT_LEN)
    {
        osal_printk("[sle_client_find_property_cbk] uuid: 0x%02x 0x%02x.\r\n", property->uuid.uuid[14],
                    property->uuid.uuid[15]); 
    }
    else if (property->uuid.len == UUID_128BIT_LEN)
    {
        for (uint16_t idx = 0; idx < UUID_128BIT_LEN; idx++)
        {
            osal_printk("[sle_client_find_property_cbk] uuid [0x%x]: 0x%02x.\r\n", idx, property->uuid.uuid[idx]);
        }
    }

    g_sle_send_param.handle = property->handle;
    g_sle_send_param.type = SSAP_PROPERTY_TYPE_VALUE; 
}

static void sle_client_find_structure_cmp_cbk(uint8_t client_id, uint16_t conn_id,
                                              ssapc_find_structure_result_t *structure_result,
                                              errcode_t status)
{
    unused(conn_id);

    osal_printk("%s [sle_client_find_structure_cmp_cbk] client id:%d status:%d type:%d uuid len:%d \r\n",
                SLE_CLIENT_LOG, client_id, status, structure_result->type, structure_result->uuid.len);
    if (structure_result->uuid.len == UUID_16BIT_LEN)
    {
        osal_printk("[sle_client_find_structure_cmp_cbk] structure uuid:[0x%02x][0x%02x]\r\n",
                    structure_result->uuid.uuid[14], structure_result->uuid.uuid[15]); 
    }
    else
    {
        for (uint8_t idx = 0; idx < UUID_128BIT_LEN; idx++)
        {
            osal_printk("[sle_client_find_structure_cmp_cbk] structure uuid[0x%x]:[0x%02x]\r\n", idx,
                        structure_result->uuid.uuid[idx]);
        }
    }
}

static void sle_client_write_cfm_cbk(uint8_t client_id, uint16_t conn_id,
                                     ssapc_write_result_t *write_result, errcode_t status)
{
    osal_printk("%s [sle_client_write_cfm_cbk] conn_id:%d client id:%d status:%d handle:%02x type:%02x\r\n",
                SLE_CLIENT_LOG, conn_id, client_id, status, write_result->handle, write_result->type);
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

errcode_t sle_client_send_report_by_handle(const uint8_t *data, uint8_t len)
{
    
    ssapc_write_param_t param = {0};  

    param.handle = g_sle_find_service_result.start_hdl;

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

    if(SsapWriteReq(g_client_id, g_sle_conn_id, &param) != ERRCODE_SUCC)
    {
        printf("[sle_client_send_report_by_handle] SsapWriteReq fail\r\n");
        osal_vfree(param.data);
        return ERRCODE_FAIL;
    }

    osal_vfree(param.data);
    return ERRCODE_SUCC;
}

int sle_client_send_data(uint8_t *data, uint8_t length)
{
    int ret;
    ret = sle_client_send_report_by_handle(data, length); 
    return ret;
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

void sle_client_init()
{
    
    uint8_t local_addr[SLE_ADDR_LEN] = {0x13, 0x67, 0x5c, 0x07, 0x00, 0x51}; 

    sle_addr_t local_address;

    local_address.type = SLE_ADDRESS_TYPE_PUBLIC;

    (void)memcpy_s(local_address.addr, SLE_ADDR_LEN, local_addr, SLE_ADDR_LEN);

    sle_uuid_client_register();

    sle_client_seek_cbk_register();

    sle_client_connect_cbk_register();

    sle_client_ssapc_cbk_register(ssapc_notification_cbk, ssapc_indication_cbk);

    EnableSle();

    sle_set_local_addr(&local_address);
}

void sle_wait_service_found(void)
{
    while (g_sle_service_found == 0)
    {
        osal_msleep(100); 
    }
}

static void SleTask(char *arg)
{
    (void)arg;

    sle_set_server_name("hybrid_n_node");

    sle_client_init();

    sle_wait_service_found();

    char *data = "Hello from SLE client!\n";
    while (1)
    {
        int ret = sle_client_send_data((uint8_t *)data, strlen(data));
        osDelay(100);
    }
}

#define SLE_UUID_CLIENT_TASK_PRIO 26           // SLE客户端任务优先级
#define SLE_UUID_CLIENT_STACK_SIZE 0x2000    // SLE客户端任务栈大小
#define SLE_SEND_DATA_TASK_PRIO 27           // 数据发送任务优先级
#define SLE_SEND_DATA_STACK_SIZE 0x1000      // 数据发送任务栈大小

/**
 * @brief SLE UUID客户端程序入口函数
 * @note 创建两个线程：SLE客户端初始化线程和数据发送线程
 */
static void sle_uuid_client_entry(void)
{
    osal_task *task_handle = NULL;
    osal_kthread_lock();


    task_handle= osal_kthread_create((osal_kthread_handler)SleTask, 0, "send_data_task",
        SLE_SEND_DATA_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, SLE_SEND_DATA_TASK_PRIO);
        osal_kfree(task_handle);
    }
    osal_kthread_unlock();
}

/* 运行应用入口函数 */
app_run(sle_uuid_client_entry);