/**
 * Copyright (c) dt-sir
 *
 * Description: callbacks register. \n
 *              This file is to register all sle callbacks which were used. \n
 *              ���ļ�ʵ����SLEЭ�����лص�������ע�����֧�ֻ��ģʽ�µĿͻ��˺ͷ���˹��ܡ�\n
 *
 * History: \n
 * 2025-04-08, Create file. \n
 */
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

// ȫ�ֻص��ṹ�嶨�壬���ڴ洢�豸���֡��㲥��������صĻص�����
sle_announce_seek_callbacks_t g_sle_seek_cbk = {0};  // �豸���ֺ͹㲥�ص��ṹ��
sle_connection_callbacks_t g_sle_connect_cbk = {0};  // ���ӹ���ص��ṹ��

// �ⲿ������������Щ�����ֱ��ڿͻ��˺ͷ����ģ����ʵ��
// �������ػص�����
extern void sle_server_sle_enable_cbk(errcode_t status);
extern void sle_client_sle_enable_cbk(errcode_t status);
extern void sle_server_sle_disable_cbk(errcode_t status);
extern void sle_client_sle_disable_cbk(errcode_t status);
extern void sle_server_announce_enable_cbk(uint32_t announce_id, errcode_t status);
extern void sle_server_announce_disable_cbk(uint32_t announce_id, errcode_t status);
extern void sle_server_announce_terminal_cbk(uint32_t announce_id);
// �ͻ�����ػص�����
extern void sle_client_sample_seek_enable_cbk(errcode_t status);
extern void sle_client_sample_seek_disable_cbk(errcode_t status);
extern void sle_client_sample_seek_result_info_cbk(sle_seek_result_info_t *seek_result_data);

/**
 * @brief SLEʹ�ܻص�����
 * @param status �������״̬��
 * @note ͬʱ��������˺Ϳͻ��˵�SLEʹ�ܻص���ʵ�ֻ��ģʽ�µ�ͬ����ʼ��
 */
static void sle_enable_cbk(errcode_t status)
{
    sle_server_sle_enable_cbk(status);  
    sle_client_sle_enable_cbk(status);
}

/**
 * @brief SLE���ûص�����
 * @param status �������״̬��
 * @note ͬʱ��������˺Ϳͻ��˵�SLE���ûص���ʵ�ֻ��ģʽ�µ�ͬ���ر�
 */
static void sle_disable_cbk(errcode_t status)
{
    sle_server_sle_disable_cbk(status); 
    sle_client_sle_disable_cbk(status); 
}

/**
 * @brief ע���豸���ֺ͹㲥��صĻص�����
 * @return �������״̬��
 * @note ͬʱע������(�㲥)�Ϳͻ���(ɨ��)�����лص�������һ��ȫ�ֽṹ����
 */
static errcode_t sle_announce_seek_register_cbks(void)
{
    errcode_t ret;
    // ע��SLE�����ص�
    g_sle_seek_cbk.sle_enable_cb = sle_enable_cbk;
    g_sle_seek_cbk.sle_disable_cb = sle_disable_cbk;

    // ע�����˹㲥��ػص�
    g_sle_seek_cbk.announce_enable_cb = sle_server_announce_enable_cbk;
    g_sle_seek_cbk.announce_disable_cb = sle_server_announce_disable_cbk;
    g_sle_seek_cbk.announce_terminal_cb = sle_server_announce_terminal_cbk;

    // ע��ͻ���ɨ����ػص�
    g_sle_seek_cbk.seek_enable_cb = sle_client_sample_seek_enable_cbk;
    g_sle_seek_cbk.seek_result_cb = sle_client_sample_seek_result_info_cbk;
    g_sle_seek_cbk.seek_disable_cb = sle_client_sample_seek_disable_cbk;

    ret = sle_announce_seek_register_callbacks(&g_sle_seek_cbk);
    return ret; 
}

// �ⲿ�����������������ӹ�����ػص�
// �����������ػص�
extern void sle_server_connect_state_changed_cbk(uint16_t conn_id, const sle_addr_t *addr,
    sle_acb_state_t conn_state, sle_pair_state_t pair_state, sle_disc_reason_t disc_reason);
// �ͻ���������ػص�
extern void sle_client_connect_state_changed_cbk(uint16_t conn_id, const sle_addr_t *addr,
    sle_acb_state_t conn_state, sle_pair_state_t pair_state, sle_disc_reason_t disc_reason);
extern void sle_client_pair_complete_cbk(uint16_t conn_id, const sle_addr_t *addr, errcode_t status);
// ���Ӳ������º���������˻ص�
extern void sle_server_connect_param_update_req_cbk(uint16_t conn_id, errcode_t status, const sle_connection_param_update_req_t *param);
extern void sle_server_connect_param_update_cbk(uint16_t conn_id, errcode_t status,
    const sle_connection_param_update_evt_t *param);
extern void sle_server_auth_complete_cbk(uint16_t conn_id, const sle_addr_type_t *addr, errcode_t status, const sle_auth_info_evt_t *evt);
extern void sle_server_read_rssi_cbk(uint16_t conn_id, int8_t rssi, errcode_t status);
void sle_server_pair_complete_cbk(uint16_t conn_id, const sle_addr_t *addr, errcode_t status);

/**
 * @brief ����״̬�仯�ص�����
 * @param conn_id ����ID
 * @param addr �豸��ַ
 * @param conn_state ����״̬
 * @param pair_state ���״̬
 * @param disc_reason �Ͽ�ԭ��
 * @note ͨ���Ƚ�MAC��ַ�ж��Ƿ���˻��ǿͻ��˵����ӣ������ö�Ӧ�Ļص�����
 */
static void connect_state_changed_cbk(uint16_t conn_id, const sle_addr_t *addr,
    sle_acb_state_t conn_state, sle_pair_state_t pair_state, sle_disc_reason_t disc_reason)
{
    osal_printk("[connect_state_changed_cbk] mac: %02x:%02x:%02x:%02x:%02x:%02x\r\n",
           addr->addr[0], addr->addr[1], addr->addr[2], addr->addr[3], addr->addr[4], addr->addr[5]);

    // ��ȡԶ�̷�������ַ�������ж���������
    sle_addr_t *remote_server_addr = sle_get_remote_server_addr();
    if(memcmp(addr,remote_server_addr,sizeof(sle_addr_t)) != 0)
    {
        // ��ַ��ƥ��Զ�̷���������Ϊ����˴��������
        osal_printk("\r\n[connect cbk]: addr != remote-server-addr\r\n");
        sle_server_connect_state_changed_cbk(conn_id, addr, conn_state, pair_state, disc_reason);
    }
    else 
    {
        // ��ַƥ��Զ�̷���������Ϊ�ͻ��˴��������
        osal_printk("\r\n[connect cbk]: addr == remote-server-addr\r\n");
        sle_client_connect_state_changed_cbk(conn_id, addr, conn_state, pair_state, disc_reason);
    }
}

/**
 * @brief �����ɻص�����
 * @param conn_id ����ID
 * @param addr �豸��ַ
 * @param status ��Խ��״̬��
 * @note ͬ��ͨ���Ƚ�MAC��ַ�ж��Ƿ���˻��ǿͻ��˵���ԣ������ö�Ӧ�Ļص�����
 */
static void sle_pair_complete_cbk(uint16_t conn_id, const sle_addr_t *addr, errcode_t status)
{
    osal_printk("[sle_pair_complete_cbk] mac: %02x:%02x:%02x:%02x:%02x:%02x\r\n",
           addr->addr[0], addr->addr[1], addr->addr[2], addr->addr[3], addr->addr[4], addr->addr[5]);

    // ��ȡԶ�̷�������ַ�������ж��������
    sle_addr_t *remote_server_addr = sle_get_remote_server_addr();
    if(memcmp(addr,remote_server_addr,sizeof(sle_addr_t)) != 0)
    {
        // ��ַ��ƥ��Զ�̷���������Ϊ����˴�������
        osal_printk("\r\n[connect cbk]: addr != remote-server-addr\r\n");
        sle_server_pair_complete_cbk(conn_id, addr,status);
    }
    else 
    {
        // ��ַƥ��Զ�̷���������Ϊ�ͻ��˴�������
        osal_printk("\r\n[connect cbk]: addr == remote-server-addr\r\n");
        sle_client_pair_complete_cbk(conn_id, addr, status);
    }
}

/**
 * @brief ע�����ӹ�����صĻص�����
 * @return �������״̬��
 * @note ���������ӹ�����صĻص�����ע�ᵽȫ�ֽṹ����
 */
static errcode_t sle_conn_register_cbks(void)
{
    errcode_t ret;
    // ע������״̬�仯�ص����������ַܷ�������˻�ͻ��˵�ͳһ���
    g_sle_connect_cbk.connect_state_changed_cb = connect_state_changed_cbk;

    // ע�����Ӳ���������ػص�
    g_sle_connect_cbk.connect_param_update_cb = sle_server_connect_param_update_cbk;
    g_sle_connect_cbk.connect_param_update_req_cb = sle_server_connect_param_update_req_cbk;
    //g_sle_connect_cbk.auth_complete_cb = sle_server_auth_complete_cbk; // ��ǰδʹ����֤��ɻص�
    
    // ע�������ɺ��ź�ǿ�Ȼص�
    g_sle_connect_cbk.pair_complete_cb = sle_pair_complete_cbk;
    g_sle_connect_cbk.read_rssi_cb = sle_server_read_rssi_cbk;

    // ��SLEЭ��ջע�����ӻص��ṹ��
    ret = sle_connection_register_callbacks(&g_sle_connect_cbk);
    return ret;
}

/**
 * @brief ע������SLEͨ�ûص�����
 * @return �������״̬��
 * @note ���ǻ��ģʽ�б��ⲿ���õ���Ҫ�ӿں���������ע������SLEЭ����Ҫ�Ļص�����
 */
errcode_t sle_register_common_cbks(void)
{
    errcode_t ret;

    // ע���豸���ֺ͹㲥��ػص�
    ret = sle_announce_seek_register_cbks();
    if(ret != ERRCODE_SUCC)
    {
        osal_printk("[register cbks]: sle_announce_seek_register_cbks FAIL\r\n");
        return ret;
    }

    // ע�����ӹ�����ػص�
    ret = sle_conn_register_cbks();
    if(ret != ERRCODE_SUCC)
    {
        osal_printk("[register cbks]: sle_conn_register_cbks FAIL\r\n");
        return ret;
    }

    return ERRCODE_SUCC;
}
