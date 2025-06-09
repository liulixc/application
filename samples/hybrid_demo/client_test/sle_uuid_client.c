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

#define SLE_MTU_SIZE_DEFAULT        300        // 默认MTU大小，用于定义数据包的最大传输单元
#define SLE_SEEK_INTERVAL_DEFAULT   100        // 默认搜索间隔，单位毫秒
#define SLE_SEEK_WINDOW_DEFAULT     100        // 默认搜索窗口，单位毫秒
#define UUID_16BIT_LEN 2                       // 16位UUID长度，2字节
#define UUID_128BIT_LEN 16                     // 128位UUID长度，16字节

// 全局回调函数结构体和连接相关变量
sle_announce_seek_callbacks_t g_seek_cbk = {0};      // 搜索相关回调函数
sle_connection_callbacks_t    g_connect_cbk = {0};   // 连接相关回调函数
ssapc_callbacks_t             g_ssapc_cbk = {0};     // SSAP客户端回调函数
sle_addr_t                    g_remote_addr = {0};   // 远程设备地址
uint16_t                      g_conn_id = 0;         // 连接ID
ssapc_find_service_result_t   g_find_service_result = {0}; // 服务查找结果
sle_addr_t local_addr = {0};                         // 本地设备地址

/**
 * @brief SLE启用回调函数
 * @param status 操作状态码
 * @note 当SLE启用成功后开始扫描设备
 */
void sle_sample_sle_enable_cbk(errcode_t status)
{
    if (status == 0) {
        sle_start_scan();
    }
}

/**
 * @brief 搜索启用回调函数
 * @param status 操作状态码
 */
void sle_sample_seek_enable_cbk(errcode_t status)
{
    if (status == 0) {
        return;
    }
}

/**
 * @brief 搜索停止回调函数
 * @param status 操作状态码
 * @note 当搜索停止后尝试连接远程设备
 */
void sle_sample_seek_disable_cbk(errcode_t status)
{
    if (status == 0) {
        sle_connect_remote_device(&g_remote_addr);
    }
}

/**
 * @brief 搜索结果处理回调函数
 * @param seek_result_data 搜索结果数据
 * @note 当找到设备后保存其地址并停止搜索
 */
void sle_sample_seek_result_info_cbk(sle_seek_result_info_t *seek_result_data)
{
    if (seek_result_data != NULL) {
        (void)memcpy_s(&g_remote_addr, sizeof(sle_addr_t), &seek_result_data->addr, sizeof(sle_addr_t));
        sle_stop_seek();
    }
}

/**
 * @brief 注册搜索相关回调函数
 */
void sle_sample_seek_cbk_register(void)
{
    g_seek_cbk.sle_enable_cb = sle_sample_sle_enable_cbk;
    g_seek_cbk.seek_enable_cb = sle_sample_seek_enable_cbk;
    g_seek_cbk.seek_disable_cb = sle_sample_seek_disable_cbk;
    g_seek_cbk.seek_result_cb = sle_sample_seek_result_info_cbk;
}

/**
 * @brief 连接状态变化回调函数
 * @param conn_id 连接ID
 * @param addr 设备地址
 * @param conn_state 连接状态
 * @param pair_state 配对状态
 * @param disc_reason 断开原因
 * @note 当连接成功且未配对时，尝试与远程设备配对
 */
void sle_sample_connect_state_changed_cbk(uint16_t conn_id, const sle_addr_t *addr,
    sle_acb_state_t conn_state, sle_pair_state_t pair_state, sle_disc_reason_t disc_reason)
{
    osal_printk("[ssap client] conn state changed conn_id:%d, addr:%02x***%02x%02x\n", conn_id, addr->addr[0],
        addr->addr[4], addr->addr[5]); /* 0 4 5: addr index */
    osal_printk("[ssap client] conn state changed disc_reason:0x%x\n", disc_reason);
    if (conn_state == SLE_ACB_STATE_CONNECTED) {
        if (pair_state == SLE_PAIR_NONE) {
            sle_pair_remote_device(&g_remote_addr);
        }
        g_conn_id = conn_id;
    }
}

/**
 * @brief 配对完成回调函数
 * @param conn_id 连接ID
 * @param addr 设备地址
 * @param status 操作状态码
 * @note 配对成功后，交换MTU信息
 */
void sle_sample_pair_complete_cbk(uint16_t conn_id, const sle_addr_t *addr, errcode_t status)
{
    osal_printk("[ssap client] pair complete conn_id:%d, addr:%02x***%02x%02x\n", conn_id, addr->addr[0],
        addr->addr[4], addr->addr[5]); /* 0 4 5: addr index */
    if (status == 0) {
        ssap_exchange_info_t info = {0};
        info.mtu_size = SLE_MTU_SIZE_DEFAULT;
        info.version = 1;
        ssapc_exchange_info_req(0, g_conn_id, &info);
    }
}

/**
 * @brief 注册连接相关回调函数
 */
void sle_sample_connect_cbk_register(void)
{
    g_connect_cbk.connect_state_changed_cb = sle_sample_connect_state_changed_cbk;
    g_connect_cbk.pair_complete_cb = sle_sample_pair_complete_cbk;
}

/**
 * @brief 交换信息回调函数
 * @param client_id 客户端ID
 * @param conn_id 连接ID
 * @param param 交换参数
 * @param status 操作状态码
 * @note 信息交换后，开始查找服务
 */
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

/**
 * @brief 查找结构回调函数
 * @param client_id 客户端ID
 * @param conn_id 连接ID
 * @param service 服务信息
 * @param status 操作状态码
 * @note 保存找到的服务信息
 */
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
    memcpy_s(&g_find_service_result.uuid, sizeof(sle_uuid_t), &service->uuid, sizeof(sle_uuid_t));
}

/**
 * @brief 查找结构完成回调函数
 * @param client_id 客户端ID
 * @param conn_id 连接ID
 * @param structure_result 结构查询结果
 * @param status 操作状态码
 * @note 查找服务完成后，发送初始数据到服务
 */
//这个函数可以更改发送数据
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
    uint8_t data[] = {0xAA, 0xBB, 0xCC, 0xDD};
    uint8_t len = sizeof(data);
    ssapc_write_param_t param = {0};
    param.handle = g_find_service_result.start_hdl;
    param.type = SSAP_PROPERTY_TYPE_VALUE;
    param.data_len = len;
    param.data = data;
    ssapc_write_req(0, conn_id, &param);
}

/**
 * @brief 查找属性回调函数
 * @param client_id 客户端ID
 * @param conn_id 连接ID
 * @param property 属性信息
 * @param status 操作状态码
 * @note 打印找到的属性信息
 */
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

/**
 * @brief 写确认回调函数
 * @param client_id 客户端ID
 * @param conn_id 连接ID
 * @param write_result 写操作结果
 * @param status 操作状态码
 * @note 写操作完成后，尝试读取同一属性
 */
void sle_sample_write_cfm_cbk(uint8_t client_id, uint16_t conn_id, ssapc_write_result_t *write_result,
    errcode_t status)
{   
    osal_printk("[ssap client] write cfm cbk, client id: %d status:%d.\n", client_id, status);
    ssapc_read_req(0, conn_id, write_result->handle, write_result->type);
}

/**
 * @brief 读确认回调函数
 * @param client_id 客户端ID
 * @param conn_id 连接ID
 * @param read_data 读取的数据
 * @param status 操作状态码
 * @note 打印读取到的数据内容
 */
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

/**
 * @brief 通知回调函数
 * @param client_id 客户端ID
 * @param conn_id 连接ID
 * @param data 接收到的数据
 * @param status 操作状态码
 * @note 处理从服务端收到的通知数据
 */
void sle_sample_notification_cbk(uint8_t client_id, uint16_t conn_id, ssapc_handle_value_t *data,
    errcode_t status)
{
    unused(client_id);
    unused(conn_id);
    unused(status);
    osal_printk("recv:data_len:%d\r\n",data->data_len);
    osal_printk("recv data : %s",data->data);
}

/**
 * @brief 注册SSAP客户端回调函数
 * @note 将各种SSAP操作的回调函数注册到回调结构体中
 */
void sle_sample_ssapc_cbk_register(void)
{
    g_ssapc_cbk.exchange_info_cb = sle_sample_exchange_info_cbk;
    g_ssapc_cbk.find_structure_cb = sle_sample_find_structure_cbk;
    g_ssapc_cbk.find_structure_cmp_cb = sle_sample_find_structure_cmp_cbk;
    g_ssapc_cbk.ssapc_find_property_cbk = sle_sample_find_property_cbk;
    g_ssapc_cbk.write_cfm_cb = sle_sample_write_cfm_cbk;
    g_ssapc_cbk.read_cfm_cb = sle_sample_read_cfm_cbk;
    g_ssapc_cbk.notification_cb = sle_sample_notification_cbk;

}

/**
 * @brief 设置本地设备地址
 * @note 配置设备的MAC地址为公共类型地址：01:02:03:04:05:06
 */
void sle_set_addr(void)
{
    uint8_t local_addr[SLE_ADDR_LEN] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06}; 

    osal_printk("Set local_addr: %02x:%02x:%02x:%02x:%02x:%02x\r\n",
           local_addr[0], local_addr[1], local_addr[2], local_addr[3], local_addr[4], local_addr[5]);
    sle_addr_t local_address;
    local_address.type = SLE_ADDRESS_TYPE_PUBLIC;
    memcpy_s(local_address.addr,SLE_ADDR_LEN,local_addr,SLE_ADDR_LEN);
    sle_set_local_addr(&local_address);
}

/**
 * @brief SLE客户端初始化函数
 * @note 注册各种回调函数，设置设备地址，并启用SLE功能
 */
void sle_client_init()
{
    sle_sample_seek_cbk_register();
    sle_sample_connect_cbk_register();
    sle_sample_ssapc_cbk_register();
    sle_announce_seek_register_callbacks(&g_seek_cbk);
    sle_connection_register_callbacks(&g_connect_cbk);
    ssapc_register_callbacks(&g_ssapc_cbk);
    sle_set_addr();
    sle_get_local_addr(&local_addr);
    osal_printk("get local_addr: %02x:%02x:%02x:%02x:%02x:%02x\r\n",
           local_addr.addr[0], local_addr.addr[1], local_addr.addr[2], local_addr.addr[3], local_addr.addr[4], local_addr.addr[5]);
    enable_sle();
}

/**
 * @brief 开始扫描设备
 * @note 设置扫描参数并启动扫描过程
 */
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

/**
 * @brief 数据发送任务
 * @note 周期性向已连接的设备发送测试数据，每秒发送一次固定格式的数据
 */
void send_data_task(void)
{
    osal_msleep(10000);
    errcode_t ret = 0;
    uint8_t data[] = {0x22, 0x88, 0xEE, 0x99};
    uint8_t len = sizeof(data);
    ssapc_write_param_t param = {0};
    param.handle = g_find_service_result.start_hdl;
    param.type = SSAP_PROPERTY_TYPE_VALUE;
    param.data_len = len;
    param.data = data;
    while(1)
    {
        ret = ssapc_write_req(0, g_conn_id, &param);
        if(ret != ERRCODE_SUCC)
        {
            osal_printk("write FAIL\r\n");
        }
        else 
        {
            osal_printk("write SUCC\r\n");
        }
        osal_msleep(1000);
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
    task_handle= osal_kthread_create((osal_kthread_handler)sle_client_init, 0, "sle_gatt_client",
        SLE_UUID_CLIENT_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, SLE_UUID_CLIENT_TASK_PRIO);
        osal_kfree(task_handle);
    }

    task_handle= osal_kthread_create((osal_kthread_handler)send_data_task, 0, "send_data_task",
        SLE_SEND_DATA_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, SLE_SEND_DATA_TASK_PRIO);
        osal_kfree(task_handle);
    }
    osal_kthread_unlock();
}

/* 运行应用入口函数 */
app_run(sle_uuid_client_entry);