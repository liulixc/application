/**
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2023-2023. All rights reserved.
 *
 * Description: SLE Mesh Gateway Client.
 *
 */
 #ifndef SLE_GATEWAY_CLIENT_H
 #define SLE_GATEWAY_CLIENT_H
 
 #include "sle_ssap_client.h"
 #include <stdbool.h>
 
 /**
  * @brief �ڵ��ɫ����
  */
 typedef enum {
     NODE_ROLE_ORPHAN = 0, // �¶��ڵ㣬�ȴ�������
     NODE_ROLE_MEMBER = 1, // ��Ա�ڵ㣬������
     NODE_ROLE_GATEWAY = 2, // ���ؽڵ�
 } node_role_t;
 
 /**
  * @brief �Զ��峧�̹㲥���ݽṹ
  */
 typedef struct {
     uint8_t len;         // ���ݳ���
     uint8_t type;        // ���� (0xFF ��ʾ�����Զ�������)
     uint16_t company_id; // ����ID
     uint8_t role;        // �ڵ��ɫ (node_role_t)
     uint8_t level;       // �ڵ��������еĲ㼶
 } __attribute__((packed)) network_adv_data_t;
 
 /**
  * @brief "����"����ṹ��
  */
 #define ADOPTION_CMD 0x01 // ������������Ĳ�����
 typedef struct {
     uint8_t cmd;   // ������ (ADOPTION_CMD)
     uint8_t level; // ���ڵ�Ĳ㼶
 } __attribute__((packed)) adoption_cmd_t;
 
 /**
  * @brief �����ϱ��ṹ��
  */
  typedef struct {
    uint8_t origin_mac[6]; // ԭʼ���ݽڵ��MAC��ַ
    char    data[];        // �ɱ䳤���� (����JSON�ַ���)
} __attribute__((packed)) report_data_t;
 
 // �ӽڵ�������Ϣ����ṹ��
 typedef struct {
     uint16_t conn_id;       // ����ID
     bool is_active;         // �Ƿ��Ծ
     uint8_t mac[6];         // �ӽڵ��MAC��ַ
     uint16_t write_handle;  // Ϊ���ӽڵ�洢�Ŀ�д���
 } child_node_info_t;
 
 // �豸ӳ��ṹ��
 typedef struct {
     uint16_t conn_id;           // �豸ID
     uint8_t device_index;       // g_env_msg�е��豸����
     bool is_active;             // �豸�Ƿ��Ծ
     uint8_t mac[6];      // �豸MAC��ַ
     char cloud_device_id[64];   // �豸ID (�� "680b91649314d11851158e8d_Battery05")
 } bms_device_map_t;
 
 // ��ʼ������
 void sle_gateway_client_init(void);
 
 // ��ȡ��ǰ���ӵ��ӽڵ�����
 uint8_t get_active_children_count(void);
 
 // ��ȡ��Ծ�豸����
 uint8_t get_active_device_count(void);
 
 #endif