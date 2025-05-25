#include "l610.h"
#include "stdint.h"
#include "string.h"
#include "stdlib.h"
#include "string.h"
#include "stdio.h"

#include "uart.h"
#include "osal_debug.h"
#include "osal_task.h"
#include "securec.h"
#include "gpio.h"
#include "pinctrl.h"
#include "osal_addr.h"
#include "osal_wait.h"


// ����id
#define UART_ID UART_BUS_2
/*���ڽ��ջ�������С*/
#define UART_RX_MAX 512
uint8_t uart_rx_buffer[UART_RX_MAX];
/* ���ڽ���io*/
#define CONFIG_UART_TXD_PIN 8 
#define CONFIG_UART_RXD_PIN 7
#define CONFIG_UART_PIN_MODE 2

uart_recv uart2_recv = {0};

// ����������ATָ���շ�����
static void* l610_mutex = NULL; // osal_mutex_tδ����ʱ��void*

// ���ڻص�������������ֹ���뱨��
void uart_read_handler(const void *buffer, uint16_t length, bool error);

uint8_t isPrintf=1;	//������main����: �Ƿ��ӡ��־

#define CMD_LEN 512
char cmdSend[CMD_LEN];		//�����ϱ����ݵ�ATָ��
uint32_t DefaultTimeout=500;//��ʱ


// �������ݰ�
uint32_t uart_send_buff(uint8_t *str, uint16_t len)
{
    uint32_t ret = 0;
    ret = uapi_uart_write(UART_ID, str, len, 0xffffffff);
    if (ret != 0) {
        printf("send lenth:%d\n", ret);
    }
    return ret;
}

/* ���ڽ��ջص� */
void uart_read_handler(const void *buffer, uint16_t length, bool error)
{
    unused(error);
    uint16_t copy_len = length > sizeof(uart2_recv.recv) ? sizeof(uart2_recv.recv) : length;
    memset(uart2_recv.recv, 0, sizeof(uart2_recv.recv));
    memcpy_s(uart2_recv.recv, sizeof(uart2_recv.recv), buffer, copy_len);
    uart2_recv.recv_len = copy_len;
    uart2_recv.recv_flag = 1;
}

/* ���ڳ�ʼ������*/
void app_uart_init_config(void)
{
    uart_buffer_config_t uart_buffer_config;
    uapi_pin_set_mode(CONFIG_UART_TXD_PIN, CONFIG_UART_PIN_MODE);
    uapi_pin_set_mode(CONFIG_UART_RXD_PIN, CONFIG_UART_PIN_MODE);
    uart_attr_t attr = {
        .baud_rate = 115200, .data_bits = UART_DATA_BIT_8, .stop_bits = UART_STOP_BIT_1, .parity = UART_PARITY_NONE};
    uart_buffer_config.rx_buffer_size = 512;
    uart_buffer_config.rx_buffer = uart_rx_buffer;
    uart_pin_config_t pin_config = {.tx_pin = S_MGPIO0, .rx_pin = S_MGPIO1, .cts_pin = PIN_NONE, .rts_pin = PIN_NONE};
    uapi_uart_deinit(UART_ID);
    int res = uapi_uart_init(UART_ID, &pin_config, &attr, NULL, &uart_buffer_config);
    if (res != 0) {
        printf("uart init failed res = %02x\r\n", res);
    }
    if (uapi_uart_register_rx_callback(UART_ID, UART_RX_CONDITION_MASK_IDLE, 1, uart_read_handler) == ERRCODE_SUCC) {
        osal_printk("uart%d int mode register receive callback succ!\r\n", UART_BUS_0);
    }
}



/*
��������: L610_SendCmd
*˵��: 	L610ģ���ATָ���
*����:	uint_t *cmd����Ҫ���͵�����
*	uint8_t *result��������õĽ��
	uint32_t timeOut���ȴ����������ʱ��
	uint8_t isPrintf���Ƿ��ӡLog
*����ֵ:	��
*/
// �Ż����L610 ATָ��ͺ��������ӻ���������
void L610_SendCmd(char *cmd, char *result, uint32_t timeOut, uint8_t isPrintf) {
    if (l610_mutex) osal_mutex_lock(l610_mutex);
    uart2_recv.recv_flag = 0;
    uart2_recv.recv_len = 0;
    memset(uart2_recv.recv, 0, sizeof(uart2_recv.recv));
    uart_send_buff((uint8_t *)cmd, strlen(cmd));
    uint32_t elapsed = 0;
    int found = 0;
    while (elapsed < timeOut) {
        if (uart2_recv.recv_flag) {
            uart2_recv.recv[uart2_recv.recv_len < sizeof(uart2_recv.recv) ? uart2_recv.recv_len : sizeof(uart2_recv.recv)-1] = '\0';
            if (isPrintf) printf("[L610] receive: %s\r\n", uart2_recv.recv);
            if (strstr((char *)uart2_recv.recv, result)) {
                found = 1;
                break;
            }
            uart2_recv.recv_flag = 0; // �����־�������ȴ���һ����
        }
        osal_msleep(10);
        elapsed += 10;
    }
    if (found) {
        if (isPrintf) printf("[L610] Success!\r\n");
    } else {
        if (isPrintf) printf("[L610] Fail!\r\n");
    }
    if (l610_mutex) osal_mutex_unlock(l610_mutex);
}
/*
��������: L610_Attach
˵��:L610ģ���ʼ������
����: uint8_t isPrintf:�Ƿ��ӡLog;   uint8_t  isReboot:�Ƿ�����;
*/

void L610_Attach(uint8_t isPrintf,uint8_t isReboot) {
	if (isReboot== 1) {
		L610_SendCmd((uint8_t *) "AT+CIMI\r\n", (uint8_t *) "OK", DefaultTimeout, isPrintf);
		L610_SendCmd((uint8_t *) "AT\r\n", (uint8_t *) "OK", DefaultTimeout, isPrintf);
		L610_SendCmd((uint8_t *) "AT+CSQ\r\n", (uint8_t *) "+CSQ", DefaultTimeout,isPrintf);
		L610_SendCmd((uint8_t *) "AT+MIPCALL=1\r\n", (uint8_t *) "OK", DefaultTimeout,isPrintf);
//		L610_SendCmd((uint8_t *) "AT+****\r\n", (uint8_t *) "***", DefaultTimeout, isPrintf);
//		L610_SendCmd((uint8_t *) "AT+****\r\n", (uint8_t *) "***", DefaultTimeout, isPrintf);
		printf("Attach!\r\n");
	}
}

/*
��������: L610_Detach
˵��: L610ģ���������Ͽ��������ӣ�
����: uint8_t isPrintf: �Ƿ��ӡLog
*/
void L610_Detach(uint8_t isPrintf) {
    // // �ر�TCP/UDP Socket
    // L610_SendCmd("AT+MIPCLOSE=1\r\n", "OK", DefaultTimeout, isPrintf);
    // // �ر�MQTT����
    // L610_SendCmd("AT+MQTTCLOSE=1\r\n", "OK", DefaultTimeout, isPrintf);
    //�رջ�Ϊ�����ӣ����У�
    L610_SendCmd("AT+HMDIS\r\n", "OK", DefaultTimeout, isPrintf);

    L610_SendCmd((uint8_t *) "AT+MIPCALL=0\r\n", (uint8_t *) "OK", DefaultTimeout,isPrintf);
    if (isPrintf) printf("Detach!\r\n");
}

/********************MQTTЭ��****************************/

/*
��������: L610_MQTTUSER
˵��: L610�û�����
����: Username�û�����Password�û����룬ClientIDStr�ͻ���ID
����ֵ:��
*/

void L610_MQTTUSER(char *Username, char *Password, char *ClientIDStr){
	memset(cmdSend, 0, sizeof(cmdSend));
	snprintf(cmdSend, sizeof(cmdSend), "AT+MQTTUSER=1,%s,%s,%s\r\n", Username, Password, ClientIDStr);
	L610_SendCmd(cmdSend, "OK", DefaultTimeout, isPrintf);
}

void L610_ConnetMQTT(char *server_ip, char *server_port) {
	memset(cmdSend, 0, sizeof(cmdSend));
	snprintf(cmdSend, sizeof(cmdSend), "AT+MQTTOPEN=1,%s,%s,1,60\r\n", server_ip, server_port);
	L610_SendCmd(cmdSend, "", DefaultTimeout, isPrintf);
}

void L610_MQTTSub(char *topic) {
	memset(cmdSend, 0, sizeof(cmdSend));
	snprintf(cmdSend, sizeof(cmdSend), "AT+MQTTSUB=1,%s,1\r\n", topic);
	L610_SendCmd(cmdSend, "OK", 2000, isPrintf);
}

void L610_MQTTPub(char *topic, char *msg) {
    memset(cmdSend, 0, sizeof(cmdSend));
    int len = strlen(msg);
    snprintf(cmdSend, sizeof(cmdSend), "AT+MQTTPUB=1,%s,1,0,%d\r\n", topic, len);
    L610_SendCmd(cmdSend, "", 2000, isPrintf);
    char msgSend[256] = {0};
    snprintf(msgSend, sizeof(msgSend), "%s\r\n", msg);
    L610_SendCmd(msgSend, "", 2000, isPrintf);
}

// ��Ϊ��ƽ̨MQTT����
void L610_HuaweiCloudConnect(char *ip, char *port, char *clientid, char *password, int keepalive, int cleanSession) {
    memset(cmdSend, 0, sizeof(cmdSend));
    snprintf(cmdSend, sizeof(cmdSend),
        "AT+HMCON=0,%d,\"%s\",\"%s\",\"%s\",\"%s\",%d\r\n",
        keepalive, ip, port, clientid, password, cleanSession);
    L610_SendCmd(cmdSend, "OK", 5000, isPrintf);
}

// ��Ϊ��ƽ̨�ϱ�����
void L610_HuaweiCloudReport(char *topic, char *payload) {
    memset(cmdSend, 0, sizeof(cmdSend));
    int payload_len = strlen(payload); // ֻ��ԭʼJSON����
    // ����ת����payload
    char payload_escaped[512] = {0};
    int j = 0;
    for (int i = 0; payload[i] != '\0' && j < sizeof(payload_escaped) - 1; i++) {
        if (payload[i] == '"' && j < sizeof(payload_escaped) - 2) {
            payload_escaped[j++] = '\\';
            payload_escaped[j++] = '"';
        } else {
            payload_escaped[j++] = payload[i];
        }
    }
    payload_escaped[j] = '\0';
    snprintf(cmdSend, sizeof(cmdSend),
        "AT+HMPUB=1,\"%s\",%d,\"%s\"\r\n",
        topic, payload_len, payload_escaped);
    L610_SendCmd(cmdSend, "OK", 2000, isPrintf);
}

/********************TCPЭ��****************************/
/*
��������: L610_OpenSocket
˵��: L610����IP����Socket����Server
����:server_ip,������IP   server_port,�������˿�
����ֵ:��
*/
void L610_OpenSocket_TCP(char *server_ip, char *server_port) {
	memset(cmdSend, 0, sizeof(cmdSend));
	snprintf(cmdSend, sizeof(cmdSend), "AT+MIPOPEN=1,,%s,%s,0\r\n", server_ip, server_port);
	L610_SendCmd(cmdSend, "+MIPOPEN:", DefaultTimeout, isPrintf);
}

/*��������: L610_SendMsgToTCPServer
*˵��: L610ģ�鷢����Ϣ��TCP������
*����: *msg,�����͵�����
*����ֵ:��
*/
void L610_SendMsgToTCPServer(char *msg) {
	memset(cmdSend, 0, sizeof(cmdSend));
	int len = strlen(msg);
	snprintf(cmdSend, sizeof(cmdSend), "AT+MIPSEND=1,%d\r\n", len);
	L610_SendCmd(cmdSend, "", 1000, isPrintf);
	char msgSend[256] = {0};
	snprintf(msgSend, sizeof(msgSend), "%s\r\n", msg);
	L610_SendCmd(msgSend, "", 1000, isPrintf);
}


/********************UDPЭ��****************************/
/*
��������: L610_OpenSocket
˵��: L610����IP����Socket����Server
����:server_ip,������IP   server_port,�������˿�
����ֵ:��
*/
void L610_OpenSocket_UDP(char *server_ip, char *server_port) {
	memset(cmdSend, 0, sizeof(cmdSend));
	snprintf(cmdSend, sizeof(cmdSend), "AT+MIPOPEN=1,,%s,%s,1\r\n", server_ip, server_port);
	L610_SendCmd(cmdSend, "+MIPOPEN:", DefaultTimeout, isPrintf);
}

/*��������: L610_SendMsgToTCPServer
*˵��: L610ģ�鷢����Ϣ��UDP������
*����: *msg,�����͵�����:HEX��ʽ
*����ֵ:��
*/
void L610_SendMsgToUDPServer(char *msg) {
	memset(cmdSend, 0, sizeof(cmdSend));
	int len = strlen(msg);
	snprintf(cmdSend, sizeof(cmdSend), "AT+MIPSEND=1,%d\r\n", len);
	L610_SendCmd(cmdSend, "", 1000, isPrintf);
	char msgSend[256] = {0};
	snprintf(msgSend, sizeof(msgSend), "%s\r\n", msg);
	L610_SendCmd(msgSend, "", 1000, isPrintf);
}

void L610_SendToken(char *token) {
	memset(cmdSend, 0, sizeof(cmdSend));
	snprintf(cmdSend, sizeof(cmdSend), "AT+MIPSEND=1,%s\r\n", token);
	L610_SendCmd(cmdSend, "+MIPSEND: 1,0", 1000, isPrintf);
	L610_SendCmd("AT+MIPPUSH=1\r\n", "+MIPPUSH: 1,0", 1000, isPrintf);
}




