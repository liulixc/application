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
 #include "sle_uuid_client.h"
 #include "sle_hybrid.h"
 #include "cJSON.h"
#include "gpio.h"
#include "ota_task.h"
 
 #define OCTET_BIT_LEN 8
 #define UUID_LEN_2     2
 #define UUID_16BIT_LEN 2   
 #define UUID_128BIT_LEN 16 
 #define UUID_INDEX 14   
 
 #define BT_INDEX_4     4
 #define BT_INDEX_5     5
 #define BT_INDEX_0     0
 
 #define SLE_ADV_HANDLE_DEFAULT 1
 
 #define encode2byte_little(_ptr, data) \
     do { \
         *(uint8_t *)((_ptr) + 1) = (uint8_t)((data) >> 8); \
         *(uint8_t *)(_ptr) = (uint8_t)(data); \
     } while (0)
 
 /* sle server app uuid for test */
 char g_sle_uuid_app_uuid[UUID_LEN_2] = {0x12, 0x34};
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
 
 #define SLE_SERVER_LOG "[sle server]"
 
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
 
 static void sle_uuid_print(sle_uuid_t *uuid)
 {
     if (uuid == NULL)
     {
         printf("%s uuid_print,uuid is null\r\n", SLE_SERVER_LOG);
         return;
     }
     if (uuid->len == UUID_16BIT_LEN)
     {
         printf("%s uuid: %02x %02x.\n", SLE_SERVER_LOG,
                uuid->uuid[14], uuid->uuid[15]); 
     }
     else if (uuid->len == UUID_128BIT_LEN)
     {
         printf("%s uuid: \n", SLE_SERVER_LOG); 
         printf("%s 0x%02x 0x%02x 0x%02x \n", SLE_SERVER_LOG, uuid->uuid[0], uuid->uuid[1],
                uuid->uuid[2], uuid->uuid[3]);
         printf("%s 0x%02x 0x%02x 0x%02x \n", SLE_SERVER_LOG, uuid->uuid[4], uuid->uuid[5],
                uuid->uuid[6], uuid->uuid[7]);
         printf("%s 0x%02x 0x%02x 0x%02x \n", SLE_SERVER_LOG, uuid->uuid[8], uuid->uuid[9],
                uuid->uuid[10], uuid->uuid[11]);
         printf("%s 0x%02x 0x%02x 0x%02x \n", SLE_SERVER_LOG, uuid->uuid[12], uuid->uuid[13],
                uuid->uuid[14], uuid->uuid[15]);
     }
 }
 
 
 
 
 static errcode_t sle_ssaps_register_cbks(ssaps_read_request_callback ssaps_read_request_cbk,
                                          ssaps_write_request_callback ssaps_write_request_cbk)
 {
     errcode_t ret;
     ssaps_callbacks_t ssaps_cbk = {0};
     ssaps_cbk.read_request_cb = ssaps_read_request_cbk;            
     ssaps_cbk.write_request_cb = ssaps_write_request_cbk;          
     ret = ssaps_register_callbacks(&ssaps_cbk);                    
     if (ret != ERRCODE_SLE_SUCCESS)
     {
         printf("%s [sle_ssaps_register_cbks] ssaps_register_callbacks fail :%x\r\n", SLE_SERVER_LOG,
                ret);
         return ret;
     }
     return ERRCODE_SLE_SUCCESS;
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
    property.operate_indication = SSAP_OPERATE_INDICATION_BIT_READ | SSAP_OPERATE_INDICATION_BIT_NOTIFY;
    sle_uuid_setu2(SLE_UUID_SERVER_NTF_REPORT, &property.uuid);
    property.value = (uint8_t *)osal_vmalloc(sizeof(g_sle_property_value));
    if (property.value == NULL) {
        return ERRCODE_SLE_FAIL;
    }
    if (memcpy_s(property.value, sizeof(g_sle_property_value), g_sle_property_value,
        sizeof(g_sle_property_value)) != EOK) {
        osal_vfree(property.value);
        return ERRCODE_SLE_FAIL;
    }
    ret = ssaps_add_property_sync(g_server_id, g_service_handle, &property,  &g_property_handle);
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_vfree(property.value);
        return ERRCODE_SLE_FAIL;
    }
    descriptor.permissions = SLE_UUID_TEST_DESCRIPTOR;
    descriptor.type = SSAP_DESCRIPTOR_USER_DESCRIPTION;
    descriptor.operate_indication = SSAP_OPERATE_INDICATION_BIT_READ | SSAP_OPERATE_INDICATION_BIT_WRITE;
    descriptor.value = ntf_value;
    descriptor.value_len = sizeof(ntf_value);

    ret = ssaps_add_descriptor_sync(g_server_id, g_service_handle, g_property_handle, &descriptor);
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_vfree(property.value);
        osal_vfree(descriptor.value);
        return ERRCODE_SLE_FAIL;
    }
    osal_vfree(property.value);
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
 errcode_t sle_uuid_server_send_report_by_handle(const uint8_t *data, uint16_t len)
 {
    ssaps_ntf_ind_t param = {0};
    param.handle = g_property_handle;
    param.type = SSAP_PROPERTY_TYPE_VALUE;
    param.value = (uint8_t*)data;
    param.value_len = len;
    if (memcpy_s(param.value, param.value_len, data, len) != EOK) {
        return ERRCODE_SLE_FAIL;
    }
    return ssaps_notify_indicate(g_server_id, g_sle_conn_hdl, &param);
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
         if (hybrid_node_get_role() == NODE_ROLE_ORPHAN) {
             sle_stop_announce(SLE_ADV_HANDLE_DEFAULT);
             osal_printk("Orphan connected, stop advertising.\r\n");
         }
     }
 
     else if (conn_state == SLE_ACB_STATE_DISCONNECTED) {
        
        osal_printk("Disconnected from parent. Reverting to orphan.\r\n");
        hybrid_node_revert_to_orphan();

     }
 }
 
 
 
 
 static void ssaps_read_request_cbk(uint8_t server_id, uint16_t conn_id, ssaps_req_read_cb_t *read_cb_para,
     errcode_t status)
 {
     osal_printk("[uuid server] ssaps read request cbk server_id:%x, conn_id:%x, handle:%x, status:%x\r\n",
         server_id, conn_id, read_cb_para->handle, status);
 }
 
 void ssaps_write_request_cbk(uint8_t server_id, uint16_t conn_id, ssaps_req_write_cb_t *write_cb_para,
                             errcode_t status)
{
    (void)server_id;
    (void)conn_id;
    (void)status;

    if (write_cb_para->length == sizeof(adoption_cmd_t)) {
        adoption_cmd_t *cmd = (adoption_cmd_t *)(write_cb_para->value);
        if (cmd->cmd == ADOPTION_CMD) {
            osal_printk("Adoption command received from conn_id: %d, parent_level: %d\r\n", conn_id, cmd->level);
            hybrid_node_become_member(cmd->level);
            return;
        }
    }
    
    // 创建一个安全的字符串缓冲区
    char *json_str = (char*)osal_vmalloc(write_cb_para->length + 1);
    if (json_str == NULL) {
        printf("[ssaps_write_request_cbk] Memory allocation failed\r\n");
        return;
    }
    
    // 复制数据并确保以null结尾
    memcpy(json_str, write_cb_para->value, write_cb_para->length);
    json_str[write_cb_para->length] = '\0';
    
    // 移除可能的换行符和空白字符
    char *end = json_str + strlen(json_str) - 1;
    while (end > json_str && (*end == '\n' || *end == '\r' || *end == ' ' || *end == '\t')) {
        *end = '\0';
        end--;
    }
    
    printf("[ssaps_write_request_cbk] client_send_data: %s\r\n\r\n", json_str);
    
    // 解析JSON命令
    cJSON *json = cJSON_Parse(json_str);
    if (json == NULL) {
        printf("[ssaps_write_request_cbk] JSON parse failed\r\n");
        osal_vfree(json_str);
        return;
    }
    
    // 检查命令类型
    cJSON *command_name = cJSON_GetObjectItem(json, "command_name");
    if (!cJSON_IsString(command_name)) {
        printf("[ssaps_write_request_cbk] Missing or invalid command_name\r\n");
        cJSON_Delete(json);
        osal_vfree(json_str);
        return;
    }
    
    // 检查是否为ota_upgrade命令
    if (strcmp(command_name->valuestring, "ota_upgrade") == 0) {
        printf("[ssaps_write_request_cbk] OTA upgrade command received\r\n");
        ota_task_start();
        cJSON_Delete(json);
        osal_vfree(json_str);
        return;
    }
    
    // 检查是否为switch命令
    if (strcmp(command_name->valuestring, "switch") != 0) {
        printf("[ssaps_write_request_cbk] Unknown command: %s\r\n", command_name->valuestring);
        cJSON_Delete(json);
        osal_vfree(json_str);
        return;
    }
    
    // 解析paras对象
    cJSON *paras = cJSON_GetObjectItem(json, "paras");
    if (!cJSON_IsObject(paras)) {
        printf("[ssaps_write_request_cbk] Missing or invalid paras object\r\n");
        cJSON_Delete(json);
        osal_vfree(json_str);
        return;
    }
    
    // 解析index
    cJSON *index = cJSON_GetObjectItem(paras, "index");
    if (!cJSON_IsString(index)) {
        printf("[ssaps_write_request_cbk] Missing or invalid index\r\n");
        cJSON_Delete(json);
        osal_vfree(json_str);
        return;
    }
    
    // 解析switch
    cJSON *switch_val = cJSON_GetObjectItem(paras, "switch");
    if (!cJSON_IsString(switch_val)) {
        printf("[ssaps_write_request_cbk] Missing or invalid switch value\r\n");
        cJSON_Delete(json);
        osal_vfree(json_str);
        return;
    }
    
    // 获取本机MAC地址后两位
    sle_addr_t *local_addr = hybrid_get_local_addr();
    char local_mac_str[3];
    snprintf(local_mac_str, sizeof(local_mac_str), "%02X", local_addr->addr[5]);
    
    printf("[ssaps_write_request_cbk] Command index: %s, Local MAC last 2: %s, Switch: %s\r\n", 
           index->valuestring, local_mac_str, switch_val->valuestring);
    
    // 判断index是否与本机MAC地址后两位相同
    if (strcmp(index->valuestring, local_mac_str) == 0) {
        // 匹配，执行继电器控制命令
        printf("[ssaps_write_request_cbk] Index matches local MAC, executing relay control\r\n");
        
        int switch_state = atoi(switch_val->valuestring);
        if (switch_state == 1) {
            // 打开继电器
            uapi_gpio_set_val(13, GPIO_LEVEL_HIGH);
            uapi_gpio_set_val(14, GPIO_LEVEL_HIGH);
            printf("[ssaps_write_request_cbk] Relay turned ON\r\n");
        } else {
            // 关闭继电器
            uapi_gpio_set_val(13, GPIO_LEVEL_LOW);
            uapi_gpio_set_val(14, GPIO_LEVEL_LOW);
            printf("[ssaps_write_request_cbk] Relay turned OFF\r\n");
        }
    } else {
        // 不匹配，转发给子节点
        printf("[ssaps_write_request_cbk] Index does not match, forwarding to child nodes\r\n");

        sle_client_send_command_to_children(write_cb_para->value, write_cb_para->length);
    }
    
    cJSON_Delete(json);
    osal_vfree(json_str);
}
 

 
 //****************hybrid******************//
 errcode_t sle_hybrids_init()
 {
     errcode_t ret;
 
     ret = sle_ssaps_register_cbks(ssaps_read_request_cbk, ssaps_write_request_cbk);
     if (ret != ERRCODE_SLE_SUCCESS)
     {
         printf("%s [sle_hybrids_init] sle_ssaps_register_cbks fail :%x\r\n", SLE_SERVER_LOG, ret);
         return ret;
     }
 
     printf("%s [sle_hybrids_init] init ok\r\n", SLE_SERVER_LOG);
     return ERRCODE_SLE_SUCCESS;
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
 
 int sle_hybrids_send_data(uint8_t *data, uint16_t length)
 {
     int ret;
     ret = sle_uuid_server_send_report_by_handle(data, length);
    if (ret != ERRCODE_SUCC)
    {
        osal_printk("[sle_hybrids_send_data] send data fail, ret:%d\r\n", ret);
        return ret;
    }
     return ret;
 }
 
 uint8_t sle_hybrids_is_client_connected(void)
 {
     if (g_sle_hybrids_conn_state == SLE_ACB_STATE_CONNECTED)
     {
         return 1;
     }
     else
     {
         return 0;
     }
 }
