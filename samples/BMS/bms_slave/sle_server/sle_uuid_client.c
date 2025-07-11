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
 #include "sle_uuid_server.h" // For network definitions
 #include "sle_hybrid.h" // For node state functions
 
 #undef THIS_FILE_ID
 #define THIS_FILE_ID BTH_GLE_SAMPLE_UUID_CLIENT//这是干嘛的，没看懂一点ty
 
 /*==============================================================================
  * 配置参数定义区域
  *============================================================================*/
 #define SLE_MTU_SIZE_DEFAULT        512
 #define SLE_SEEK_INTERVAL_DEFAULT   100
 #define SLE_SEEK_WINDOW_DEFAULT     100
 #define MAX_CHILDREN                2
 #define MAX_REMOTE_SERVERS 2
 
 #define UUID_16BIT_LEN 2
 #define UUID_128BIT_LEN 16
 
 #define SLE_CLIENT_LOG "[sle client]" 
 
 
 // 网关自身状态
 static const node_role_t g_my_role = NODE_ROLE_GATEWAY;
 static const uint8_t g_my_level = 0;
 
 // 子节点管理
 static child_node_info_t g_child_nodes[MAX_CHILDREN] = {0};
 static uint8_t g_active_children_count = 0;
 
 // SLE协议栈回调函数集
 static sle_announce_seek_callbacks_t g_sle_seek_cbk = {0};
 static sle_connection_callbacks_t g_sle_connect_cbk = {0};
 static ssapc_callbacks_t g_sle_ssapc_cbk = {0};
 
 // 连接过程控制变量
 static sle_addr_t g_connecting_addr = {0};
 static bool g_is_connecting = false;
 static uint8_t g_client_id = 0;
 
 // 远程服务器地址管理
 static sle_addr_t g_remote_server_addrs[MAX_REMOTE_SERVERS] = {0};
 static uint8_t g_remote_server_count = 0;
 
 /*==============================================================================
  * 内部函数声明
  *============================================================================*/
 void sle_start_scan(void);
 static void sle_hybridc_adopt_child(uint16_t conn_id);
 static int find_child_index_by_conn_id(uint16_t conn_id);
 static void sle_client_find_property_cbk(uint8_t client_id, uint16_t conn_id,
                                           ssapc_find_property_result_t *property, errcode_t status);
 static void sle_client_find_structure_cmp_cbk(uint8_t client_id, uint16_t conn_id,
                                                ssapc_find_structure_result_t *structure_result,
                                                errcode_t status);
 
 // char g_sle_server_name[128] = "hi_mesh_node";
 
 /*==============================================================================
  * 子节点管理函数
  *============================================================================*/
 static int allocate_child_node(uint16_t conn_id, const sle_addr_t *addr)
 {
     if (g_active_children_count >= MAX_CHILDREN) {
         osal_printk("%s Max children reached.\r\n", SLE_CLIENT_LOG);
         return -1;
     }
     for (int i = 0; i < MAX_CHILDREN; i++) {
         if (!g_child_nodes[i].is_active) {
             g_child_nodes[i].is_active = true;
             g_child_nodes[i].conn_id = conn_id;
             (void)memcpy_s(g_child_nodes[i].mac, sizeof(g_child_nodes[i].mac), addr->addr, sizeof(addr->addr));
             g_child_nodes[i].write_handle = 0; // 初始化句柄
             g_active_children_count++;
             osal_printk("%s Allocated child at index %d for conn_id %u.\r\n", SLE_CLIENT_LOG, i, conn_id);
             return i;
         }
     }
     return -1;
 }
 
 static void remove_child_node_by_conn_id(uint16_t conn_id)
 {
     for (int i = 0; i < MAX_CHILDREN; i++) {
         if (g_child_nodes[i].is_active && g_child_nodes[i].conn_id == conn_id) {
             g_child_nodes[i].is_active = false;
             g_child_nodes[i].conn_id = 0;
             g_active_children_count--;
             osal_printk("%s Removed child at index %d. Active children: %u\r\n", SLE_CLIENT_LOG, i, g_active_children_count);
             break;
         }
     }
 }
 
 uint8_t get_active_children_count(void)
 {
     return g_active_children_count;
 }
 
 
 // sle_addr_t *sle_get_remote_server_addr(void)
 // {
 //     return &g_sle_remote_server_addr;
 // }
 
 // uint8_t sle_get_num_remote_server_addr(void)
 // {
 //     return g_num_remote_server_addr;
 // }
 
 // void sle_set_server_name(char *name)
 // {
 //     memcpy_s(g_sle_server_name, strlen(name), name, strlen(name));
 // }
 
 
 
 
 void sle_client_sle_enable_cbk(errcode_t status)
 {
     // 由 sle_hybrid.c 控制何时开始扫描
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
 
     if (status == ERRCODE_SUCC && g_is_connecting) {
         osal_printk("%s Scan stopped, connecting to orphan...\r\n", SLE_CLIENT_LOG);
         sle_connect_remote_device(&g_connecting_addr);
     }
 }
 
 void sle_client_sample_seek_result_info_cbk(sle_seek_result_info_t *seek_result_data)
 {
     if (seek_result_data == NULL || g_is_connecting || g_active_children_count >= MAX_CHILDREN) {
         return;
     }
 
     for (uint16_t i = 0; i < seek_result_data->data_length; ) {
         uint8_t len = seek_result_data->data[i];
         if (len == 0 || (i + len) >= seek_result_data->data_length) break;
         
         uint8_t type = seek_result_data->data[i + 1];
         if (type == 0xFF && len >= sizeof(network_adv_data_t) - 1) {
             network_adv_data_t *net_data = (network_adv_data_t *)&seek_result_data->data[i];
             if (net_data->role == NODE_ROLE_ORPHAN) {
                 osal_printk("%s Orphan found! Stopping scan to connect.\r\n", SLE_CLIENT_LOG);
                 g_is_connecting = true;
                 (void)memcpy_s(&g_connecting_addr, sizeof(sle_addr_t), &seek_result_data->addr, sizeof(sle_addr_t));
                 
                 // 添加到远程服务器地址列表
                 add_remote_server_addr(&seek_result_data->addr);
                 
                 sle_stop_seek();
                 return;
             }
         }
         i += len + 1;
     }
     
 }
 
 void sle_client_connect_state_changed_cbk(uint16_t conn_id, const sle_addr_t *addr,
     sle_acb_state_t conn_state, sle_pair_state_t pair_state, sle_disc_reason_t disc_reason)
 {
     osal_printk("%s [Child Conn] State changed. conn_id:%d, state:%d\r\n", SLE_CLIENT_LOG, conn_id, conn_state);
     g_is_connecting = false;
     if (conn_state == SLE_ACB_STATE_CONNECTED) {
         osal_printk("%s Connected successfully to conn_id %u.\r\n", SLE_CLIENT_LOG, conn_id);
         if (allocate_child_node(conn_id, addr) < 0) {
             sle_disconnect_remote_device(conn_id);
             return;
         }
         ssap_exchange_info_t info = { .mtu_size = SLE_MTU_SIZE_DEFAULT, .version = 1 };
         ssapc_exchange_info_req(g_client_id, conn_id, &info);
     } else if (conn_state == SLE_ACB_STATE_DISCONNECTED) {
         osal_printk("%s Disconnected from conn_id %u, reason: 0x%x\r\n", SLE_CLIENT_LOG, conn_id, disc_reason);
         remove_child_node_by_conn_id(conn_id);
         // 从远程服务器地址列表中移除
         remove_remote_server_addr(addr);

        if (g_active_children_count < MAX_CHILDREN) {
            if(hybrid_node_get_role() != NODE_ROLE_ORPHAN) {
                // 如果当前是孤儿节点，重新开始扫描
                sle_start_scan();
            } 
        }
         
     }
    
 }
 
 void sle_client_exchange_info_cbk(uint8_t client_id, uint16_t conn_id, ssap_exchange_info_t *param,
     errcode_t status)
 {
     osal_printk("%s Exchange info cbk, conn_id:%u, status:%d, MTU:%d\r\n", SLE_CLIENT_LOG, conn_id, status, param->mtu_size);
     if (status == ERRCODE_SUCC) {
         // MTU交换成功，现在开始服务发现
         osal_printk("%s Starting service discovery for conn_id %u.\r\n", SLE_CLIENT_LOG, conn_id);
         ssapc_find_structure_param_t find_param = {
             .type = SSAP_FIND_TYPE_PROPERTY, // 查找特征(Property)
             .start_hdl = 1,
             .end_hdl = 0xFFFF,
         };
         ssapc_find_structure(g_client_id, conn_id, &find_param);
     } else {
        //  sle_disconnect_remote_device(conn_id);
        printf("disconnect all remote device\r\n");
     }
 }
 
 
 static void sle_client_find_property_cbk(uint8_t client_id, uint16_t conn_id,
                                          ssapc_find_property_result_t *property, errcode_t status)
 {
     unused(client_id);
     if (status == ERRCODE_SUCC) {
         // 根据子设备的服务端定义，我们要找的特征UUID是0x1122
         // 由于字节序，它在内存中是 {..., 0x22, 0x11}
         if (property->uuid.len == 2 && property->uuid.uuid[14] == 0x22 && property->uuid.uuid[15] == 0x11) {
             osal_printk("%s Found correct property handle %u for conn_id %u\r\n", SLE_CLIENT_LOG, property->handle, conn_id);
             int child_idx = find_child_index_by_conn_id(conn_id);
             if (child_idx != -1) {
                 g_child_nodes[child_idx].write_handle = property->handle;
             }
         }
     }
 }
 
static void sle_client_find_structure_cmp_cbk(uint8_t client_id, uint16_t conn_id,
                                               ssapc_find_structure_result_t *structure_result,
                                               errcode_t status)
{
     unused(client_id);
     unused(structure_result);
     osal_printk("%s Find structure complete, conn_id:%u, status:%d\r\n", SLE_CLIENT_LOG, conn_id, status);
     if (status == ERRCODE_SUCC) {
         int child_idx = find_child_index_by_conn_id(conn_id);
         if (child_idx != -1 && g_child_nodes[child_idx].write_handle != 0) {
             // 成功找到句柄，现在可以"收养"了
             sle_hybridc_adopt_orphan(conn_id,hybrid_node_get_level());
         } else {
             osal_printk("%s Did not find writable handle for conn_id %u. Disconnecting.\r\n", SLE_CLIENT_LOG, conn_id);
             sle_disconnect_remote_device(conn_id);
         }
     }
 }
 
 static void sle_client_write_cfm_cb(uint8_t client_id, uint16_t conn_id,
                                      ssapc_write_result_t *write_result, errcode_t status)
 {
    if (g_active_children_count < MAX_CHILDREN) {
            sle_start_scan();
    }
     osal_printk("%s Adoption write confirm, conn_id:%u, status:%d\r\n", SLE_CLIENT_LOG, conn_id, status);
 }
 
 void ssapc_notification_cbk(uint8_t client_id, uint16_t conn_id, ssapc_handle_value_t *data,
    errcode_t status)
{
    (void)client_id;
    if (status != ERRCODE_SUCC || data == NULL || data->data == NULL) {
        osal_printk("%s Notification error or empty data. conn_id:%u, status:%d\r\n", SLE_CLIENT_LOG, conn_id, status);
        return;
    }

    // 确保收到的数据是安全的字符串，以便打印
    data->data[data->data_len - 1] = '\0';

    // 打印从子节点收到的JSON字符串
    osal_printk("%s [DATA RECV] from child conn_id %u. Data: %s\r\n",SLE_CLIENT_LOG,conn_id,(char*)data->data);

    // 如果当前节点是“成员”，则必须将数据包原封不动地转发给父节点
    if (hybrid_node_get_role() == NODE_ROLE_MEMBER) {
        osal_printk("%s [DATA FORWARD] Relaying data to parent...\r\n", SLE_CLIENT_LOG);
        // 转发收到的原始数据包（即JSON字符串）
        errcode_t ret = sle_hybrids_send_data(data->data, data->data_len);
        if (ret != ERRCODE_SUCC) {
            osal_printk("%s Failed to forward data to parent. Error: %d\r\n", SLE_CLIENT_LOG, ret);
        }
    }
}
 
 void ssapc_indication_cbk(uint8_t client_id, uint16_t conn_id, ssapc_handle_value_t *data,
                           errcode_t status)
 {
     // Indication不用于数据上报，目前仅打印
     (void)client_id;
     (void)conn_id;
     (void)status;
     if (data != NULL && data->data != NULL) {
         osal_printk("[ssapc_indication_cbk] Indication received, len: %u\r\n", data->data_len);
     }
 }
 
 static void sle_client_ssapc_cbk_register(void)
 {
     g_sle_ssapc_cbk.exchange_info_cb = sle_client_exchange_info_cbk;
     g_sle_ssapc_cbk.ssapc_find_property_cbk = sle_client_find_property_cbk;
     g_sle_ssapc_cbk.find_structure_cmp_cb = sle_client_find_structure_cmp_cbk;
     g_sle_ssapc_cbk.write_cfm_cb = sle_client_write_cfm_cb;
     g_sle_ssapc_cbk.notification_cb = ssapc_notification_cbk;
     g_sle_ssapc_cbk.indication_cb = ssapc_indication_cbk;
     ssapc_register_callbacks(&g_sle_ssapc_cbk);
 }
 
 static errcode_t sle_uuid_client_register(void)
 {
     sle_uuid_t app_uuid = { .len = 2, .uuid = {0x12, 0x34} }; // 客户端使用一个虚拟UUID
     errcode_t ret = ssapc_register_client(&app_uuid, &g_client_id);
     if (ret != ERRCODE_SUCC) {
         osal_printk("%s ssapc_register_client failed: 0x%x\r\n", SLE_CLIENT_LOG, ret);
     }
 }
 
 
 /*==============================================================================
  * 辅助函数
  *============================================================================*/
 void sle_start_scan()
 {
      if (g_is_connecting) return;
     osal_printk("%s Starting scan...\r\n", SLE_CLIENT_LOG);
     sle_seek_param_t param = {
         .own_addr_type = SLE_ADDRESS_TYPE_PUBLIC,
         .filter_duplicates = 1,
         .seek_filter_policy = SLE_SEEK_FILTER_ALLOW_ALL,
         .seek_phys = SLE_SEEK_PHY_1M,
         .seek_type = {SLE_SEEK_ACTIVE},
         .seek_interval = {SLE_SEEK_INTERVAL_DEFAULT},
         .seek_window = {SLE_SEEK_WINDOW_DEFAULT}
     };
     sle_set_seek_param(&param);
     sle_start_seek();
 }
 
 /* 新增函数：根据连接ID查找子节点索引 */
 static int find_child_index_by_conn_id(uint16_t conn_id)
 {
     for (int i = 0; i < MAX_CHILDREN; i++) {
         if (g_child_nodes[i].is_active && g_child_nodes[i].conn_id == conn_id) {
             return i;
         }
     }
     return -1;
 }
 
 //*************hybrid******************//
 // errcode_t sle_client_send_report_by_handle(const uint8_t *data, uint8_t len)
 // {
     
 //     ssapc_write_param_t param = {0};  
 
 //     int ret = 0;
 //     for (int i = 0; i < g_num_conn; i++)
 //     {
         
 //         if (g_conn_and_service_arr[i].find_service_result.start_hdl == 0)
 //         {
 //             continue; 
 //         }
 
 //         param.handle = g_conn_and_service_arr[i].find_service_result.start_hdl;
 
 //         param.type = SSAP_PROPERTY_TYPE_VALUE;
 
 //         param.data_len = len + 1; 
 
 //         param.data = osal_vmalloc(param.data_len);
 //         if (param.data == NULL)
 //         {
 //             printf("[sle_client_send_report_by_handle] osal_vmalloc fail\r\n");
 //             return ERRCODE_FAIL;
 //         }
 //         if (memcpy_s(param.data, param.data_len, data, len) != EOK)
 //         {
 //             osal_vfree(param.data);
 //             return ERRCODE_FAIL;
 //         }
 
 //         ret = SsapWriteReq(g_client_id, g_conn_and_service_arr[i].conn_id, &param);
 //         if (ret != ERRCODE_SUCC)
 //         {
 //             printf("SsapWriteReq error:%d connid:%d\r\n", ret, g_conn_and_service_arr[i].conn_id);
 //         }
 
 //         osal_vfree(param.data);
 //     }
 
 //     return ERRCODE_SUCC;
 // }
 
 // int sle_hybridc_send_data(uint8_t *data, uint8_t length)
 // {
 //     int ret;
 //     ret = sle_client_send_report_by_handle(data, length); 
 //     return ret;
 // }
 
 void sle_hybridc_init()
 {
     
     uint32_t ret = sle_uuid_client_register();
     printf("sle_uuid_client_register_errcode:%d\r\n", ret);
 
     sle_client_ssapc_cbk_register();
 }
 
 /**
  * @brief 客户端发送"收养"命令
  * @param conn_id 目标子节点的连接ID
  * @param parent_level 当前节点（父节点）的网络层级
  */
 void sle_hybridc_adopt_orphan(uint16_t conn_id, uint8_t parent_level)
 {
     int child_idx = find_child_index_by_conn_id(conn_id);
     if (child_idx == -1 || g_child_nodes[child_idx].write_handle == 0) {
         osal_printk("%s Cannot adopt, no valid handle for conn_id %u\r\n", SLE_CLIENT_LOG, conn_id);
         return;
     }
 
     adoption_cmd_t cmd = { .cmd = ADOPTION_CMD, .level = parent_level };
     ssapc_write_param_t param = {
         .handle = g_child_nodes[child_idx].write_handle, // 使用发现到的正确句柄
         .type = SSAP_PROPERTY_TYPE_VALUE,
         .data_len = sizeof(cmd),
         .data = (uint8_t *)&cmd,
     };
     errcode_t ret = SsapWriteReq(g_client_id, conn_id, &param);
     if (ret != ERRCODE_SUCC) {
         osal_printk("%s Failed to send adoption command: %d\r\n", SLE_CLIENT_LOG, ret);
     } else {
         osal_printk("%s Adoption command sent to conn_id %u.\r\n", SLE_CLIENT_LOG, conn_id);
     }
 }
 
 
 /**
  * @brief 断开所有子节点的连接
  */
 void sle_hybridc_disconnect_all_children(void)
 {
     osal_printk("Disconnecting all %d children.\r\n", g_active_children_count);
 
     if (g_active_children_count == 0) {
         return;
     }

    // 获取远程服务器地址并进行判断连接类型
    sle_addr_t *remote_server_addrs = sle_get_remote_server_addrs();
    uint8_t remote_server_count = sle_get_remote_server_count();

    for (uint8_t i = 0; i < remote_server_count; i++)
    {
        sle_disconnect_remote_device(&remote_server_addrs[i]); // 遍历所有子节点，断开连接
    }

         
 }
 
 
 
 // 添加以下函数来管理远程服务器地址
 void add_remote_server_addr(const sle_addr_t *addr)
 {
     if (g_remote_server_count >= MAX_REMOTE_SERVERS) {
         osal_printk("%s Cannot add more remote servers, max reached.\r\n", SLE_CLIENT_LOG);
         return;
     }
     
     // 检查地址是否已存在
     for (uint8_t i = 0; i < g_remote_server_count; i++) {
         if (memcmp(addr->addr, g_remote_server_addrs[i].addr, SLE_ADDR_LEN) == 0) {
             return; // 地址已存在，无需添加
         }
     }
     
     // 添加新地址
     g_remote_server_addrs[g_remote_server_count].type = addr->type;
     memcpy_s(g_remote_server_addrs[g_remote_server_count].addr, SLE_ADDR_LEN, 
              addr->addr, SLE_ADDR_LEN);
     g_remote_server_count++;
     
     // 调试输出
     osal_printk("%s Added remote server: %02x:%02x:%02x:%02x:%02x:%02x, count: %d\r\n", 
                SLE_CLIENT_LOG, addr->addr[0], addr->addr[1], addr->addr[2], 
                addr->addr[3], addr->addr[4], addr->addr[5], g_remote_server_count);
 }
 
 void remove_remote_server_addr(const sle_addr_t *addr)
 {
     for (uint8_t i = 0; i < g_remote_server_count; i++) {
         if (memcmp(addr->addr, g_remote_server_addrs[i].addr, SLE_ADDR_LEN) == 0) {
             // 找到匹配的地址，将最后一个地址移到当前位置
             if (i < g_remote_server_count - 1) {
                 g_remote_server_addrs[i].type = g_remote_server_addrs[g_remote_server_count - 1].type;
                 memcpy_s(g_remote_server_addrs[i].addr, SLE_ADDR_LEN,
                          g_remote_server_addrs[g_remote_server_count - 1].addr, SLE_ADDR_LEN);
             }
             g_remote_server_count--;
             
             // 调试输出
             osal_printk("%s Removed remote server: %02x:%02x:%02x:%02x:%02x:%02x, count: %d\r\n", 
                        SLE_CLIENT_LOG, addr->addr[0], addr->addr[1], addr->addr[2], 
                        addr->addr[3], addr->addr[4], addr->addr[5], g_remote_server_count);
             return;
         }
     }
 }
 
 // 提供给外部模块的接口函数
 sle_addr_t* sle_get_remote_server_addrs(void)
 {
     return g_remote_server_addrs;
 }
 
 uint8_t sle_get_remote_server_count(void)
 {
     return g_remote_server_count;
 }
