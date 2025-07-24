/*==============================================================================
 * 系统头文件包含区域
 *============================================================================*/
 #include "common_def.h"
 #include "soc_osal.h"
 #include "securec.h"
 #include "sle_device_discovery.h"
 #include "sle_connection_manager.h"
 #include "sle_ssap_client.h"
 #include "sle_client.h"
 #include "cJSON.h"
 #include "mqtt_demo.h"
 
 /*==============================================================================
  * 配置参数定义区域
  *============================================================================*/
 #define SLE_MTU_SIZE_DEFAULT        512
 #define SLE_SEEK_INTERVAL_DEFAULT   100
 #define SLE_SEEK_WINDOW_DEFAULT     100
 #define SLE_GATEWAY_LOG             "[SLE Gateway]"
 #define MAX_CHILDREN                2
 
 /*==============================================================================
  * 全局变量定义区域
  *============================================================================*/
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
 sle_addr_t local_addr = {0};
 
 ssapc_find_service_result_t   g_find_service_result = {0};
 
 static bool g_is_connecting = false;
 static uint8_t g_client_id = 0;

 // 外部全局变量引用
extern volatile environment_msg g_env_msg[MAX_BMS_DEVICES];        // 全局环境消息变量，存储BMS数据


uint8_t g_active_device_count = 0;  // 当前活跃设备数量

bool is_device_active[12] = {false}; // 设备活跃状态数组
 
 
 /*==============================================================================
  * 内部函数声明
  *============================================================================*/
 static void sle_gateway_start_scan(void);
 static void sle_gateway_adopt_child(uint16_t conn_id);
 static int find_child_index_by_conn_id(uint16_t conn_id);
 static void sle_gateway_find_property_cbk(uint8_t client_id, uint16_t conn_id,
                                           ssapc_find_property_result_t *property, errcode_t status);
 static void sle_gateway_find_structure_cmp_cbk(uint8_t client_id, uint16_t conn_id,
                                                ssapc_find_structure_result_t *structure_result,
                                                errcode_t status);
 
 /*==============================================================================
  * 子节点管理函数
  *============================================================================*/
 static int allocate_child_node(uint16_t conn_id, const sle_addr_t *addr)
 {
     if (g_active_children_count >= MAX_CHILDREN) {
         osal_printk("%s Max children reached.\r\n", SLE_GATEWAY_LOG);
         return -1;
     }
     for (int i = 0; i < MAX_CHILDREN; i++) {
         if (!g_child_nodes[i].is_active) {
             g_child_nodes[i].is_active = true;
             g_child_nodes[i].conn_id = conn_id;
             (void)memcpy_s(g_child_nodes[i].mac, sizeof(g_child_nodes[i].mac), addr->addr, sizeof(addr->addr));
             g_child_nodes[i].write_handle = 0; // 初始化句柄
             g_active_children_count++;
             osal_printk("%s Allocated child at index %d for conn_id %u.\r\n", SLE_GATEWAY_LOG, i, conn_id);
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
             osal_printk("%s Removed child at index %d. Active children: %u\r\n", SLE_GATEWAY_LOG, i, g_active_children_count);
             break;
         }
     }
 }
 
 uint8_t get_active_children_count(void)
 {
     return g_active_children_count;
 }
 
 uint8_t get_active_device_count(void)
 {
     uint8_t count = 0;
     for (int i = 0; i < 12; i++) {
         if (is_device_active[i]) {
             count++;
         }
     }
     return count;
 }
 
 /*==============================================================================
  * SLE 客户端回调函数
  *============================================================================*/
 static void sle_gateway_enable_cbk(errcode_t status)
 {
     osal_printk("%s SLE enabled, status: %d.\r\n", SLE_GATEWAY_LOG, status);
     if (status == ERRCODE_SUCC) {
         sle_gateway_start_scan();
     }
 }
 
 void sle_gateway_seek_enable_cbk(errcode_t status)
 {
     if (status == 0) {
         return;
     }
 }
 

 
 static void sle_gateway_seek_result_cbk(sle_seek_result_info_t *seek_result_data)
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
                 osal_printk("%s Orphan found! Stopping scan to connect.\r\n", SLE_GATEWAY_LOG);
                 g_is_connecting = true;
                 (void)memcpy_s(&g_connecting_addr, sizeof(sle_addr_t), &seek_result_data->addr, sizeof(sle_addr_t));
                 sle_stop_seek();
                 return;
             }
         }
         i += len + 1;
     }
 }

  static void sle_gateway_seek_disable_cbk(errcode_t status)
 {
     if (status == ERRCODE_SUCC && g_is_connecting) {
         osal_printk("%s Scan stopped, connecting to orphan...\r\n", SLE_GATEWAY_LOG);
         sle_connect_remote_device(&g_connecting_addr);
     }
 }
 
 void sle_sample_seek_cbk_register(void)
 {
     g_sle_seek_cbk.sle_enable_cb = sle_gateway_enable_cbk;
     g_sle_seek_cbk.seek_result_cb = sle_gateway_seek_result_cbk;
     g_sle_seek_cbk.seek_disable_cb = sle_gateway_seek_disable_cbk;
     g_sle_seek_cbk.seek_enable_cb= sle_gateway_seek_enable_cbk;
 }
 
 
 static void sle_gateway_connect_state_changed_cbk(uint16_t conn_id, const sle_addr_t *addr,
                                                              sle_acb_state_t conn_state, sle_pair_state_t pair_state,
                                                              sle_disc_reason_t disc_reason)
 {
     unused(pair_state);
     g_is_connecting = false;
     
     if (conn_state == SLE_ACB_STATE_CONNECTED) {
         osal_printk("%s Connected successfully to conn_id %u.\r\n", SLE_GATEWAY_LOG, conn_id);
         if (allocate_child_node(conn_id, addr) < 0) {
             sle_disconnect_remote_device(conn_id);
             return;
         }
         ssap_exchange_info_t info = { .mtu_size = SLE_MTU_SIZE_DEFAULT, .version = 1 };
         ssapc_exchange_info_req(g_client_id, conn_id, &info);
     } else if (conn_state == SLE_ACB_STATE_DISCONNECTED) {
         osal_printk("%s Disconnected from conn_id %u, reason: 0x%x\r\n", SLE_GATEWAY_LOG, conn_id, disc_reason);
         remove_child_node_by_conn_id(conn_id);
        if (g_active_children_count < MAX_CHILDREN) {
            sle_gateway_start_scan();
        }
     }
     
    if (g_active_children_count < MAX_CHILDREN) {
            sle_gateway_start_scan();
    }

 }
 

 
 void sle_sample_connect_cbk_register(void)
 {
     g_sle_connect_cbk.connect_state_changed_cb = sle_gateway_connect_state_changed_cbk;
 }
 
 
 static void sle_gateway_exchange_info_cbk(uint8_t client_id, uint16_t conn_id, ssap_exchange_info_t *param, errcode_t status)
 {
     osal_printk("%s Exchange info cbk, conn_id:%u, status:%d, MTU:%d\r\n", SLE_GATEWAY_LOG, conn_id, status, param->mtu_size);
 
     if (status == ERRCODE_SUCC) {
         // MTU交换成功，现在开始服务发现
         osal_printk("%s Starting service discovery for conn_id %u.\r\n", SLE_GATEWAY_LOG, conn_id);
         ssapc_find_structure_param_t find_param = {
             .type = SSAP_FIND_TYPE_PROPERTY,
             .start_hdl = 1,
             .end_hdl = 0xFFFF,
         };
         ssapc_find_structure(g_client_id, conn_id, &find_param);
     } else {
         sle_disconnect_remote_device(conn_id);
     }
 }
 
  static void sle_gateway_find_property_cbk(uint8_t client_id, uint16_t conn_id,
                                          ssapc_find_property_result_t *property, errcode_t status)
 {
     unused(client_id);
     if (status == ERRCODE_SUCC) {
         // 根据子设备的服务端定义，我们要找的特征UUID是0x1122
         // 由于字节序，它在内存中是 {..., 0x22, 0x11}
         if (property->uuid.len == 2 && property->uuid.uuid[14] == 0x22 && property->uuid.uuid[15] == 0x11) {
             osal_printk("%s Found correct property handle %u for conn_id %u\r\n", SLE_GATEWAY_LOG, property->handle, conn_id);
             int child_idx = find_child_index_by_conn_id(conn_id);
             if (child_idx != -1) {
                 g_child_nodes[child_idx].write_handle = property->handle;
             }
         }
     }
 }
 
static void sle_gateway_find_structure_cmp_cbk(uint8_t client_id, uint16_t conn_id,
                                               ssapc_find_structure_result_t *structure_result,
                                               errcode_t status)
{
     unused(client_id);
     unused(structure_result);
     osal_printk("%s Find structure complete, conn_id:%u, status:%d\r\n", SLE_GATEWAY_LOG, conn_id, status);
     if (status == ERRCODE_SUCC) {
         int child_idx = find_child_index_by_conn_id(conn_id);
         if (child_idx != -1 && g_child_nodes[child_idx].write_handle != 0) {
             // 成功找到句柄，现在可以"收养"了
             sle_gateway_adopt_child(conn_id);
         } else {
             osal_printk("%s Did not find writable handle for conn_id %u. Disconnecting.\r\n", SLE_GATEWAY_LOG, conn_id);
             sle_disconnect_remote_device(conn_id);
         }
     }
 }
 

 
 
 static void sle_gateway_write_cfm_cb(uint8_t client_id, uint16_t conn_id, ssapc_write_result_t *write_result, errcode_t status)
 {

     osal_printk("%s Adoption write confirm, conn_id:%u, status:%d\r\n", SLE_GATEWAY_LOG, conn_id, status);
 }
 
 void sle_gateway_read_cfm_cbk(uint8_t client_id, uint16_t conn_id, ssapc_handle_value_t *read_data,
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
 
/**
 * @brief 根据MAC地址获取对应的华为云平台设备ID
 * @param mac MAC地址
 * @param device_id 输出参数，存储获取到的设备ID
 * @param max_len device_id缓冲区最大长度
 * @return 0表示成功，-1表示未找到映射
 */
static int get_cloud_device_id_by_mac(const uint8_t *mac, char *device_id, size_t max_len)
{
    // 预定义的MAC地址到设备ID的映射表
    // 格式: MAC地址(6字节) -> 华为云设备ID
    static const struct {
        uint8_t mac[6];
        const char *device_id;
    } mac_to_device_id_map[] = {
        // 华为云平台预期的设备ID格式
        {{0x11, 0x22, 0x33, 0x44, 0x55, 0x01}, "680b91649314d11851158e8d_Battery01"},
        {{0x11, 0x22, 0x33, 0x44, 0x55, 0x02}, "680b91649314d11851158e8d_Battery02"},
        {{0x11, 0x22, 0x33, 0x44, 0x55, 0x03}, "680b91649314d11851158e8d_Battery03"},
        {{0x11, 0x22, 0x33, 0x44, 0x55, 0x04}, "680b91649314d11851158e8d_Battery04"},
        {{0x11, 0x22, 0x33, 0x44, 0x55, 0x05}, "680b91649314d11851158e8d_Battery05"},
        {{0x11, 0x22, 0x33, 0x44, 0x55, 0x06}, "680b91649314d11851158e8d_Battery06"},
        {{0x11, 0x22, 0x33, 0x44, 0x55, 0x07}, "680b91649314d11851158e8d_Battery07"},
        {{0x11, 0x22, 0x33, 0x44, 0x55, 0x08}, "680b91649314d11851158e8d_Battery08"},
        {{0x11, 0x22, 0x33, 0x44, 0x55, 0x09}, "680b91649314d11851158e8d_Battery09"},
        {{0x11, 0x22, 0x33, 0x44, 0x55, 0x0A}, "680b91649314d11851158e8d_Battery10"},
        {{0x11, 0x22, 0x33, 0x44, 0x55, 0x0B}, "680b91649314d11851158e8d_Battery11"},
        {{0x11, 0x22, 0x33, 0x44, 0x55, 0x0C}, "680b91649314d11851158e8d_Battery12"},
        // 添加更多映射...
    };

    // 遍历映射表查找匹配的MAC地址
    for (size_t i = 0; i < sizeof(mac_to_device_id_map) / sizeof(mac_to_device_id_map[0]); i++) {
        if (memcmp(mac, mac_to_device_id_map[i].mac, sizeof(mac_to_device_id_map[i].mac)) == 0) {
            // 找到匹配项，复制设备ID并返回
            errcode_t ret = strncpy_s(device_id, max_len, mac_to_device_id_map[i].device_id, strlen(mac_to_device_id_map[i].device_id));
            if (ret != EOK) {
                osal_printk("%s Failed to copy device_id for MAC %02X:%02X:%02X:%02X:%02X:%02X\r\n",
                    SLE_GATEWAY_LOG, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
                return -1;
            }
            return 0;
        }
    }

    osal_printk("%s Warning: No predefined mapping for MAC %02X:%02X:%02X:%02X:%02X:%02X\r\n",
           SLE_GATEWAY_LOG, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    return -1;
}


// 请将此函数替换掉文件中旧的 ssapc_notification_cbk
void ssapc_notification_cbk(uint8_t client_id, uint16_t conn_id, ssapc_handle_value_t *data,
                             errcode_t status)
{
    (void)client_id;
    (void)status;

    if (status != ERRCODE_SUCC || data == NULL || data->data == NULL) {
        osal_printk("%s Notification error or empty data. conn_id:%u, status:%d\r\n", SLE_GATEWAY_LOG, conn_id, status);
        return;
    }
    // 确保字符串正确终止
    data->data[data->data_len] = '\0';
    cJSON *root = cJSON_Parse((char *)data->data);
    if (root == NULL) {
        osal_printk("%s Failed to parse JSON from conn_id %u\r\n", SLE_GATEWAY_LOG, conn_id);
        return;
    }

    cJSON *mac = cJSON_GetObjectItem(root, "mac");
    if (!cJSON_IsString(mac) || !mac->valuestring) {
        osal_printk("%s MAC地址无效或为空\r\n", SLE_GATEWAY_LOG);
        cJSON_Delete(root);
        return;
    }

    
    // 只获取MAC地址的后8位
    uint8_t mac_last_byte;
    char *mac_str = mac->valuestring;
    sscanf(mac_str + 15, "%02hhx", &mac_last_byte); // 15 = (6-1)*3,直接定位到最后一个字节

    is_device_active[mac_last_byte] = true;

    // 数据直接存储到相同索引位置
    environment_msg *env = &g_env_msg[mac_last_byte];
    cJSON *total = cJSON_GetObjectItem(root, "total");
    if (cJSON_IsNumber(total)) {
        env->total_voltage = total->valueint;
    }

    cJSON *current = cJSON_GetObjectItem(root, "current");
    if (cJSON_IsNumber(current)) {
        env->current = current->valueint;
        if(mac_last_byte !=5) env->current = 0;
    }
    
    cJSON *soc = cJSON_GetObjectItem(root, "SOC");
    if (cJSON_IsNumber(soc)) {
        env->soc = soc->valueint;
    }

    cJSON *cell_array = cJSON_GetObjectItem(root, "cell");
    if (cJSON_IsArray(cell_array)) {
        int cell_count = cJSON_GetArraySize(cell_array);
        for (int i = 0; i < cell_count && i < 12; i++) {
            cJSON *cell_item = cJSON_GetArrayItem(cell_array, i);
            if (cJSON_IsNumber(cell_item)) {
                env->cell_voltages[i] = cell_item->valueint;
            }
        }
    }

    cJSON *temp_array = cJSON_GetObjectItem(root, "T");
    if (cJSON_IsArray(temp_array)) {
        int temp_count = cJSON_GetArraySize(temp_array);
        for (int i = 0; i < temp_count && i < 5; i++) {
            cJSON *temp_item = cJSON_GetArrayItem(temp_array, i);
            if (cJSON_IsNumber(temp_item)) {
                env->temperature[i] = temp_item->valueint;
            }
        }
    }


    cJSON_Delete(root);
}
 
 // void ssapc_indication_cbk(uint8_t client_id, uint16_t conn_id, ssapc_handle_value_t *data,
 //                           errcode_t status)
 // {
 //     (void)client_id;
 //     (void)conn_id;
 //     (void)status;
 
 //     data->data[data->data_len - 1] = '\0';
 //     printf("[ssapc_indication_cbk] server_send_data: %s\r\n", data->data);
 // }
 
 void sle_sample_ssapc_cbk_register(void)
 {
     g_sle_ssapc_cbk.exchange_info_cb = sle_gateway_exchange_info_cbk;
     g_sle_ssapc_cbk.find_structure_cmp_cb = sle_gateway_find_structure_cmp_cbk;
     g_sle_ssapc_cbk.ssapc_find_property_cbk = sle_gateway_find_property_cbk;
     g_sle_ssapc_cbk.write_cfm_cb = sle_gateway_write_cfm_cb;
     g_sle_ssapc_cbk.read_cfm_cb = sle_gateway_read_cfm_cbk;
     g_sle_ssapc_cbk.notification_cb = ssapc_notification_cbk;
 
 }
 
 /*==============================================================================
  * 辅助函数
  *============================================================================*/
 void sle_gateway_start_scan(void)
 {
     if (g_is_connecting) return;
     osal_printk("%s Starting scan...\r\n", SLE_GATEWAY_LOG);
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
 
 static void sle_gateway_adopt_child(uint16_t conn_id)
 {
     int child_idx = find_child_index_by_conn_id(conn_id);
     if (child_idx == -1 || g_child_nodes[child_idx].write_handle == 0) {
         osal_printk("%s Cannot adopt, no valid handle for conn_id %u\r\n", SLE_GATEWAY_LOG, conn_id);
         return;
     }
 
     adoption_cmd_t cmd = { .cmd = ADOPTION_CMD, .level = g_my_level };
     ssapc_write_param_t param = {
         .handle = g_child_nodes[child_idx].write_handle, // 使用发现到的正确句柄
         .type = SSAP_PROPERTY_TYPE_VALUE,
         .data_len = sizeof(cmd),
         .data = (uint8_t *)&cmd,
     };
     errcode_t ret = ssapc_write_req(g_client_id, conn_id, &param);
     if (ret != ERRCODE_SUCC) {
         osal_printk("%s Failed to send adoption command: %d\r\n", SLE_GATEWAY_LOG, ret);
     } else {
         osal_printk("%s Adoption command sent to conn_id %u.\r\n", SLE_GATEWAY_LOG, conn_id);
     }
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
 
 /*==============================================================================
  * 初始化与任务创建
  *============================================================================*/
 static void sle_callbacks_register(void)
 {
     sle_sample_seek_cbk_register();
     sle_sample_connect_cbk_register();
     sle_sample_ssapc_cbk_register();
     sle_announce_seek_register_callbacks(&g_sle_seek_cbk);
     sle_connection_register_callbacks(&g_sle_connect_cbk);
     ssapc_register_callbacks(&g_sle_ssapc_cbk);
 }
 
 static void sle_client_app_register(void)
 {
     sle_uuid_t app_uuid = { .len = 2, .uuid = {0x12, 0x34} }; // 客户端使用一个虚拟UUID
     errcode_t ret = ssapc_register_client(&app_uuid, &g_client_id);
     if (ret != ERRCODE_SUCC) {
         osal_printk("%s ssapc_register_client failed: 0x%x\r\n", SLE_GATEWAY_LOG, ret);
     }
 }
 
 void sle_gateway_client_init(void)
 {
     sle_callbacks_register();
     sle_client_app_register();
     if (enable_sle() != ERRCODE_SUCC) {
         osal_printk("%s enable_sle failed!\r\n", SLE_GATEWAY_LOG);
     }
 }
 
 #include "app_init.h"
 #define SLE_GATEWAY_TASK_STACK_SIZE         0x1000
 #define SLE_GATEWAY_TASK_PRIO               25
 
 static void *sle_gateway_task(const char *arg)
 {
     unused(arg);
     osal_msleep(1000); // 等待系统初始化
     osal_printk("%s Task started.\r\n", SLE_GATEWAY_LOG);
     
     sle_gateway_client_init();
     
     while (1) {
         osal_msleep(10000);
         osal_printk("%s Status: %u children connected.\r\n", SLE_GATEWAY_LOG, g_active_children_count);
     }
     return NULL;
 }
 
 static void sle_gateway_entry(void)
 {
     osal_task *task_handle = NULL;
     osal_kthread_lock();
 
     task_handle = osal_kthread_create((osal_kthread_handler)sle_gateway_task, 0, "SLE_Gateway_Task",
                                       SLE_GATEWAY_TASK_STACK_SIZE);
     if (task_handle != NULL) {
         osal_kthread_set_priority(task_handle, SLE_GATEWAY_TASK_PRIO);
         osal_kfree(task_handle);
     }
     osal_kthread_unlock();
 }
 
 app_run(sle_gateway_entry);