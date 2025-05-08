
/**
 * @defgroup SLE_Distribute_Network_Server
 * @ingroup  SLE_Distribute_Network
 * @brief    SLE�ֲ�ʽ��������ͷ�ļ��������˷������ص�UUID�ͽӿڡ�
 * @{
 */
#ifndef SLE_DISTRIBUTE_NETWORK_SERVER_H
#define SLE_DISTRIBUTE_NETWORK_SERVER_H

#include "sle_ssap_server.h" // ����SSAP�������ؽӿ�����

/* SLE�ֲ�ʽ��������Service��UUID������Ψһ��ʶ��BLE���� */
#define SLE_UUID_SERVER_SERVICE 0xABCD

/* SLE�ֲ�ʽ��������֪ͨ��Notify��������UUID�������ϱ����ݸ��ͻ��� */
#define SLE_UUID_SERVER_NTF_REPORT 0x1122

/* SLE�ֲ�ʽ�����������ԣ�Property��������UUID���������û��ȡ���� */
#define SLE_UUID_SERVER_PROPERTY 0x3344

#endif

// SLE_UUID_SERVER_SERVICE��SLE_UUID_SERVER_NTF_REPORT �� SLE_UUID_SERVER_PROPERTY �⼸�� UUID ֻд����λ���� 0xABCD��������Ϊ���ǲ�����16λ��2�ֽڣ�UUID ��ʽ��

// �� BLE�������͹��ģ�Э���У�UUID �����ֳ��ȣ�

// 16λ���� 0xABCD�������ڱ�׼���Զ���Ķ� UUID��ʵ��ʹ��ʱ���Զ���չΪ 128λ UUID����ʽΪ 0000xxxx-0000-1000-8000-00805F9B34FB������ xxxx �����㶨��� 16λֵ��
// 32λ�������á�
// 128λ�������Զ��� UUID��
// ��������ֻд��λ��16λ UUID������Ϊ�˼� BLE ����������Ķ��壬ʵ��Э��ջ���Զ�ת��Ϊ������ 128λ UUID�������Ҫȫ��Ψһ�Ի�ͱ�׼�������֣�����ʹ�� 128λ�Զ��� UUID��