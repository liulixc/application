
#include <stdio.h>   // ��׼�����������
#include <string.h>   // �ַ�����������
#include <unistd.h>   // POSIX��׼����
#include "app_init.h"  // Ӧ�ó�ʼ��ͷ�ļ�
#include "cmsis_os2.h"  // CMSIS-RTOS2 APIͷ�ļ�
#include "common_def.h"  // ͨ�ö���ͷ�ļ�
#include "soc_osal.h"  // SOC����ϵͳ�����ͷ�ļ�
#include "sle_device_discovery.h"  // SLE�豸�������ͷ�ļ�
#include "sle_uuid_client.h"  // SLE�ͻ������ͷ�ļ�
#include "sle_uuid_server.h"  // SLE��������ͷ�ļ�

/**
 * @brief ���Ի��ģʽ�¿ͻ��˷������ݹ���
 * @note �ȴ����ַ����ÿ100���뷢��һ�ε���������
 */
static void TestHybridCSend(void)
{
    osal_printk("Hybrid-C Send\r\n");  // ��ӡ�ͻ��˷���ģʽ������Ϣ


    char data[32] = {0};  // ���巢�����ݻ�����
    int count = 1;  // ��ʼ��������
    while (1)
    {
        // ��������ת��Ϊ�ַ���
        sprintf(data, "%d", count);
        // ͨ���ͻ��˽ӿڷ�������
        sle_hybridc_send_data((uint8_t *)data, strlen(data));
        count++;  // ����������
        osDelay(100);  // ��ʱ100����
    }
}

/**
 * @brief ���Ի��ģʽ�·���˷������ݹ���
 * @note �ȴ��ͻ������Ӻ�ÿ100���뷢��һ�ε���������
 */
static void TestHybridSSend(void)
{
    osal_printk("Hybrid-S Send\r\n");  // ��ӡ����˷���ģʽ������Ϣ

    // �ȴ��ͻ������ӵ�����ˣ�����һ����������
    sle_hybrids_wait_client_connected();

    char data[16] = {0};  // ���巢�����ݻ�����
    int count = 1;  // ��ʼ��������
    while (1)
    {
        // ��������ת��Ϊ�ַ���
        sprintf(data, "%d", count);
        // ͨ������˽ӿڷ�������
        int ret = sle_hybrids_send_data((uint8_t *)data, strlen(data));
        // ���ݷ��ͽ����ӡ��ͬ����־
        if(ret != ERRCODE_SUCC)
        {
            osal_printk("sle_hybrids_send_data FAIL\r\n");  // ����ʧ��
        }
        else
        {
            osal_printk("sle_hybrids_send_data SUCC\r\n");  // ���ͳɹ�
        }
        count++;  // ����������
        osDelay(100);  // ��ʱ100����
    }
}

// �ⲿ��������������ע��SLEͨ�ûص�����
extern errcode_t sle_register_common_cbks(void);

/**
 * @brief SLE���ģʽ��������
 * @param arg ���������δʹ��
 * @note ���γ�ʼ������ˡ��ͻ��ˣ�ע��ص��������÷��Ͳ���
 */
void sle_hybrid_task(char *arg)
{
    unused(arg);  // ����δʹ�õĲ���
    errcode_t ret = 0;  // �������״̬��
    
    // 1. ��ʼ��SLE�����
    osal_printk("[sle hybrid] sle hybrid-s init\r\n");
    sle_hybrids_init();
    // ����Զ�̷��������ƣ����ڿͻ��������ж�
    sle_set_server_name("sle_server");
    
    // 2. ��ʼ��SLE�ͻ���
    osal_printk("[sle hybrid] sle hybrid-c init\r\n");
    sle_hybridc_init();

    // 3. ע��SLEͨ�ûص�����
    sle_register_common_cbks();

    // 4. ����SLE����
    ret = enable_sle();
    if (ret != 0)
    {
        osal_printk("enable_sle fail :%x\r\n", ret);
        return;  // ����ʧ��ֱ�ӷ���
    }
    osal_printk("enable_sle succ\r\n");
    // 5. ���ÿͻ��˵�ַ
    sle_set_hybridc_addr();

    // 6. ѡ�����ģʽ���ͻ��˷��ͻ����˷���
    // TestHybridCSend();  // ��ǰע�͵�����ʹ�ÿͻ��˷��Ͳ���
    TestHybridSSend();  // ʹ�÷���˷��Ͳ���
}

// �������ȼ���ջ��С����
#define SLE_HYBRIDTASK_PRIO 24          // ���ģʽ�������ȼ�
#define SLE_HYBRID_STACK_SIZE 0x2000    // ���ģʽ����ջ��С(8KB)

/**
 * @brief SLE���ģʽ������ں���
 * @note �������ģʽ������
 */
static void sle_hybrid_entry(void)
{
    osal_task *task_handle = NULL;
    osal_kthread_lock();
    task_handle= osal_kthread_create((osal_kthread_handler)sle_hybrid_task, 0, "sle_gatt_client",
        SLE_HYBRID_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, SLE_HYBRIDTASK_PRIO);
        osal_kfree(task_handle);
    }
    osal_kthread_unlock();
}

app_run(sle_hybrid_entry);