#include "string.h"
#include "soc_osal.h"
#include "sle_errcode.h"
#include "sle_connection_manager.h"
#include "sle_device_discovery.h"
#include "sle_common.h"
#include "sle_ssap_server.h"
#include "sle_ssap_client.h"
#include "errcode.h"
#include "sle_uuid_client.h"
#include "sle_server_adv.h"
#include "sle_uuid_server.h"

// 全局回调结构体定义，用于存储设备发现、广播和连接相关的回调函数
sle_announce_seek_callbacks_t g_sle_seek_cbk = {0};  // 设备发现和广播回调结构体
sle_connection_callbacks_t g_sle_connect_cbk = {0};  // 连接管理回调结构体

// 外部函数声明，这些函数分别在客户端和服务端模块中实现
// 服务端回调函数
extern void sle_server_sle_enable_cbk(errcode_t status);
extern void sle_server_sle_disable_cbk(errcode_t status);
extern void sle_client_sle_disable_cbk(errcode_t status);

extern void sle_server_announce_enable_cbk(uint32_t announce_id, errcode_t status);
extern void sle_server_announce_disable_cbk(uint32_t announce_id, errcode_t status);
extern void sle_server_announce_terminal_cbk(uint32_t announce_id);
// 客户端回调函数
extern void sle_client_sample_seek_enable_cbk(errcode_t status);
extern void sle_client_sample_seek_disable_cbk(errcode_t status);
extern void sle_client_sample_seek_result_info_cbk(sle_seek_result_info_t *seek_result_data);

/**
 * @brief SLE使能回调函数
 * @param status 操作返回状态码
 * @note 同时处理服务端和客户端的SLE使能回调，实现混合模式下的同步初始化
 */
static void sle_enable_cbk(errcode_t status)
{
    sle_server_sle_enable_cbk(status);  
    sle_client_sle_enable_cbk(status);
}

/**
 * @brief SLE禁用回调函数
 * @param status 操作返回状态码
 * @note 同时处理服务端和客户端的SLE禁用回调，实现混合模式下的同步关闭
 */
static void sle_disable_cbk(errcode_t status)
{
    sle_server_sle_disable_cbk(status); 
    sle_client_sle_disable_cbk(status); 
}

/**
 * @brief 注册设备发现和广播相关的回调函数
 * @return 操作返回状态码
 * @note 同时注册服务端(广播)和客户端(扫描)两类回调函数到一个全局结构体中
 */
static errcode_t sle_announce_seek_register_cbks(void)
{
    errcode_t ret;
    // 注册SLE总体回调
    g_sle_seek_cbk.sle_enable_cb = sle_enable_cbk;
    g_sle_seek_cbk.sle_disable_cb = sle_disable_cbk;

    // 注册服务端广播相关回调
    g_sle_seek_cbk.announce_enable_cb = sle_server_announce_enable_cbk;
    g_sle_seek_cbk.announce_disable_cb = sle_server_announce_disable_cbk;
    g_sle_seek_cbk.announce_terminal_cb = sle_server_announce_terminal_cbk;

    // 注册客户端扫描相关回调
    g_sle_seek_cbk.seek_enable_cb = sle_client_sample_seek_enable_cbk;
    g_sle_seek_cbk.seek_result_cb = sle_client_sample_seek_result_info_cbk;
    g_sle_seek_cbk.seek_disable_cb = sle_client_sample_seek_disable_cbk;

    ret = sle_announce_seek_register_callbacks(&g_sle_seek_cbk);
    return ret; 
}

// 外部函数声明，处理连接管理相关回调
// 服务端连接相关回调
extern void sle_server_connect_state_changed_cbk(uint16_t conn_id, const sle_addr_t *addr,
    sle_acb_state_t conn_state, sle_pair_state_t pair_state, sle_disc_reason_t disc_reason);

// 客户端连接相关回调
extern void sle_client_connect_state_changed_cbk(uint16_t conn_id, const sle_addr_t *addr,
sle_acb_state_t conn_state, sle_pair_state_t pair_state, sle_disc_reason_t disc_reason);

// 连接参数更新函数，服务端回调
extern void sle_server_connect_param_update_req_cbk(uint16_t conn_id, errcode_t status, const sle_connection_param_update_req_t *param);
extern void sle_server_connect_param_update_cbk(uint16_t conn_id, errcode_t status,const sle_connection_param_update_evt_t *param);
extern void sle_server_auth_complete_cbk(uint16_t conn_id, const sle_addr_type_t *addr, errcode_t status, const sle_auth_info_evt_t *evt);
extern void sle_server_pair_complete_cbk(uint16_t conn_id, const sle_addr_t *addr, errcode_t status);
extern void sle_server_read_rssi_cbk(uint16_t conn_id, int8_t rssi, errcode_t status);


/**
 * @brief 连接状态变化回调函数
 * @param conn_id 连接ID
 * @param addr 设备地址
 * @param conn_state 连接状态
 * @param pair_state 配对状态
 * @param disc_reason 断开原因
 * @note 通过比较MAC地址判断是服务端还是客户端的连接，并调用对应的回调函数
 */
static void connect_state_changed_cbk(uint16_t conn_id, const sle_addr_t *addr,
    sle_acb_state_t conn_state, sle_pair_state_t pair_state, sle_disc_reason_t disc_reason)
{
    osal_printk("[connect_state_changed_cbk] mac: %02x:%02x:%02x:%02x:%02x:%02x\r\n",
           addr->addr[0], addr->addr[1], addr->addr[2], addr->addr[3], addr->addr[4], addr->addr[5]);

    uint8_t remote_server_addr_match = 0;
    // 获取远程服务器地址并进行判断连接类型
    sle_addr_t *remote_server_addr = sle_get_remote_server_addr();
    for (uint8_t i = 0; i < sle_get_num_remote_server_addr(); i++)
    {
        if (memcmp(addr, &remote_server_addr[i], sizeof(sle_addr_t)) == 0) 
        {
            remote_server_addr_match = 1;
            break;
        }
    }

    if (remote_server_addr_match == 0) 
    {
        sle_server_connect_state_changed_cbk(conn_id, addr, conn_state, pair_state, disc_reason);
    }
    else 
    {
        sle_client_connect_state_changed_cbk(conn_id, addr, conn_state, pair_state, disc_reason);
    }

}



/**
 * @brief 注册连接管理相关的回调函数
 * @return 操作返回状态码
 * @note 将所有连接管理相关的回调函数注册到全局结构体中
 */
static errcode_t sle_conn_register_cbks(void)
{
    errcode_t ret;
    // 注册连接状态变化回调，根据地址分发到服务端或客户端的统一处理
    g_sle_connect_cbk.connect_state_changed_cb = connect_state_changed_cbk;

    // 注册连接参数更新相关回调
    g_sle_connect_cbk.connect_param_update_cb = sle_server_connect_param_update_cbk;
    g_sle_connect_cbk.connect_param_update_req_cb = sle_server_connect_param_update_req_cbk;
    g_sle_connect_cbk.auth_complete_cb = sle_server_auth_complete_cbk; // 当前未使用认证完成回调
    
    // 注册配对完成和信号强度回调
    g_sle_connect_cbk.pair_complete_cb = sle_server_pair_complete_cbk;
    g_sle_connect_cbk.read_rssi_cb = sle_server_read_rssi_cbk;

    // 向SLE协议栈注册连接回调结构体
    ret = sle_connection_register_callbacks(&g_sle_connect_cbk);
    return ret;
}

/**
 * @brief 注册所有SLE通用回调函数
 * @return 操作返回状态码
 * @note 混合模式中被外部调用的主要接口函数，用于注册所有SLE协议需要的回调函数
 */
errcode_t sle_register_common_cbks(void)
{
    errcode_t ret;

    // 注册设备发现和广播相关回调
    ret = sle_announce_seek_register_cbks();
    if(ret != ERRCODE_SUCC)
    {
        osal_printk("[register cbks]: sle_announce_seek_register_cbks FAIL\r\n");
        return ret;
    }

    // 注册连接管理相关回调
    ret = sle_conn_register_cbks();
    if(ret != ERRCODE_SUCC)
    {
        osal_printk("[register cbks]: sle_conn_register_cbks FAIL\r\n");
        return ret;
    }

    return ERRCODE_SUCC;
}
