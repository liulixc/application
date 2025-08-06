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


// 串口id
#define UART_ID UART_BUS_2
/*串口接收缓冲区大小*/
#define UART_RX_MAX 1024
uint8_t uart_rx_buffer[UART_RX_MAX];
/* 串口接收io*/
#define CONFIG_UART_TXD_PIN 8 
#define CONFIG_UART_RXD_PIN 7
#define CONFIG_UART_PIN_MODE 2

uart_recv uart2_recv = {0};

// 互斥锁用于AT指令收发保护
static void* l610_mutex = NULL; // osal_mutex_t未定义时用void*
static uint8_t l610_ready_flag = 0; // L610模块就绪标志位: 0-未就绪, 1-已就绪

uint8_t isPrintf=1;	//定义于main函数: 是否打印日志

#define CMD_LEN 1024
char cmdSend[CMD_LEN];		//发送上报数据的AT指令
uint32_t DefaultTimeout=500;//超时


// 发送数据包
uint32_t uart_send_buff(uint8_t *str, uint16_t len)
{
    uint32_t ret = 0;
    ret = uapi_uart_write(UART_ID, str, len, 0xffffffff);
    if (ret != 0) {
        printf("send lenth:%d\n", ret);
    }
    return ret;
}

/* 串口接收回调 */
void uart_read_handler(const void *buffer, uint16_t length, bool error)
{
    unused(error);
    uint16_t copy_len = length > sizeof(uart2_recv.recv) ? sizeof(uart2_recv.recv) : length;
    memset(uart2_recv.recv, 0, sizeof(uart2_recv.recv));
    memcpy_s(uart2_recv.recv, sizeof(uart2_recv.recv), buffer, copy_len);
    uart2_recv.recv_len = copy_len;
    uart2_recv.recv_flag = 1;

    // 打印接收到的数据用于调试
    printf("[L610] Received: %.*s\r\n", copy_len, (char*)uart2_recv.recv);
    
    // 检查多种可能的就绪状态，重点匹配"+SIM READY"
    if (strstr((char*)uart2_recv.recv, "READY") ) {
        // SIM卡准备好后发送AT指令激活模块
        printf("[L610] Sending AT command to activate module\r\n");
        uart_send_buff((uint8_t *)"AT\r\n", 4);
        osal_msleep(100); // 增加延时确保命令发送完成
        l610_ready_flag = 1;
    }
}

/* 串口初始化配置*/
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
函数名称: L610_SendCmd
*说明: 	L610模组的AT指令发送
*参数:	uint_t *cmd，需要发送的命令
*	uint8_t *result，期望获得的结果
	uint32_t timeOut，等待期望结果的时间
	uint8_t isPrintf，是否打印Log
*返回值:	无
*/
// 优化后的L610 AT指令发送函数，增加互斥锁保护
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
            uart2_recv.recv_flag = 0; // 清除标志，继续等待下一个包
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
函数名称: L610_Attach
说明:L610模组初始化入网
参数: uint8_t isPrintf:是否打印Log;   uint8_t  isReboot:是否重启;
*/

void L610_Attach(uint8_t isPrintf,uint8_t isReboot) {
	// 检查模块是否已初始化
	if (!l610_ready_flag) {
		printf("[L610] Waiting for module initialization...\r\n");
		// 等待最多5秒钟模块初始化完成
		if (!L610_WaitForInit(5000)) {
			printf("[L610] Module initialization timeout, attempting attach anyway\r\n");
		}
	}
	
	if (isReboot== 1) {
		L610_SendCmd((uint8_t *) "AT+MIPCALL=1\r\n", (uint8_t *) "OK", DefaultTimeout,isPrintf);
		printf("Attach!\r\n");
	}
}

/*
函数名称: L610_Detach
说明: L610模组离网（断开网络连接）
参数: uint8_t isPrintf: 是否打印Log
*/
void L610_Detach(uint8_t isPrintf) {

    L610_SendCmd("AT+HMDIS\r\n", "OK", DefaultTimeout, isPrintf);

    L610_SendCmd((uint8_t *) "AT+MIPCALL=0\r\n", (uint8_t *) "OK", DefaultTimeout,isPrintf);
    if (isPrintf) printf("Detach!\r\n");
}

/********************MQTT协议****************************/


// 华为云平台MQTT连接
void L610_HuaweiCloudConnect(char *ip, char *port, char *clientid, char *password, int keepalive, int cleanSession) {
    memset(cmdSend, 0, sizeof(cmdSend));
    snprintf(cmdSend, sizeof(cmdSend),
        "AT+HMCON=0,%d,\"%s\",\"%s\",\"%s\",\"%s\",%d\r\n",
        keepalive, ip, port, clientid, password, cleanSession);
    L610_SendCmd(cmdSend, "OK", 5000, isPrintf);
}

// 华为云平台上报数据
void L610_HuaweiCloudReport(char *topic, char *payload) {
    memset(cmdSend, 0, sizeof(cmdSend));
    int payload_len = strlen(payload); // 只算原始JSON长度
    // 构造转义后的payload
    char payload_escaped[1024] = {0};
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

/********************TCP协议****************************/
/*
函数名称: L610_OpenSocket
说明: L610请求IP并打开Socket连接Server
参数:server_ip,服务器IP   server_port,服务器端口
返回值:无
*/
void L610_OpenSocket_TCP(char *server_ip, char *server_port) {
	memset(cmdSend, 0, sizeof(cmdSend));
	snprintf(cmdSend, sizeof(cmdSend), "AT+MIPOPEN=1,,%s,%s,0\r\n", server_ip, server_port);
	L610_SendCmd(cmdSend, "+MIPOPEN:", DefaultTimeout, isPrintf);
}

/*函数名称: L610_SendMsgToTCPServer
*说明: L610模组发送消息到TCP服务器
*参数: *msg,待发送的数据
*返回值:无
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


/********************UDP协议****************************/
/*
函数名称: L610_OpenSocket
说明: L610请求IP并打开Socket连接Server
参数:server_ip,服务器IP   server_port,服务器端口
返回值:无
*/
void L610_OpenSocket_UDP(char *server_ip, char *server_port) {
	memset(cmdSend, 0, sizeof(cmdSend));
	snprintf(cmdSend, sizeof(cmdSend), "AT+MIPOPEN=1,,%s,%s,1\r\n", server_ip, server_port);
	L610_SendCmd(cmdSend, "+MIPOPEN:", DefaultTimeout, isPrintf);
}

/*函数名称: L610_SendMsgToTCPServer
*说明: L610模组发送消息到UDP服务器
*参数: *msg,待发送的数据:HEX格式
*返回值:无
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


void L610_Reset(void) {
    if (l610_mutex) osal_mutex_lock(l610_mutex);
    l610_ready_flag = 0; // 重置就绪标志
    L610_SendCmd((uint8_t *) "AT+CFUN=1,1\r\n", (uint8_t *) "OK", DefaultTimeout, isPrintf);
    osal_msleep(5000); // 等待重启完成
    if (l610_mutex) osal_mutex_unlock(l610_mutex);
    printf("L610 Reset!\r\n");
}

/**
 * @brief 等待L610模块初始化完成
 * @param timeout_ms 超时时间(毫秒)
 * @return 1-初始化完成, 0-超时失败
 */
uint8_t L610_WaitForInit(uint32_t timeout_ms) {
    uint32_t elapsed = 0;
    printf("[L610] Waiting for module initialization...\r\n");
    
    while (elapsed < timeout_ms) {
        if (l610_ready_flag) {
            printf("[L610] Module initialization completed (SIM ready)\r\n");
            return 1;
        }
        osal_msleep(100);
        elapsed += 100;
    }
    
    printf("[L610] Module initialization timeout (SIM ready: %d)\r\n", 
           l610_ready_flag);
    return 0;
}



/**
 * @brief 4G网络批量上报BMS设备数据
 * @param gate_report_topic 网关上报主题
 * @param g_env_msg 环境消息数组
 * @param is_device_active 设备活跃状态数组
 * @param get_active_device_count 获取活跃设备数量的函数指针
 * @return 成功上报的设备数量
 */
int L610_PublishBMSDevices(const char *gate_report_topic, volatile void *g_env_msg, bool *is_device_active, uint8_t (*get_active_device_count)(void)) {
    // 外部类型定义，需要包含mqtt_demo.h或在此处重新定义
    typedef struct {
        int temperature[5]; // 温度
        int current;     // 电流
        int cell_voltages[12]; // 电池电压
        int total_voltage; // 总电压
        uint8_t soc; // SOC
        uint8_t level; // 节点层级
        char child[32]; // 子节点MAC地址后两位
    } environment_msg;
    
    volatile environment_msg *env_msg = (volatile environment_msg *)g_env_msg;
    
    // 检查是否有活跃的BMS设备连接
    uint8_t active_count = get_active_device_count();
    if (active_count > 0) {
        // 4G网络由于AT指令长度限制，采用单设备网关格式逐个上报
        int published_count = 0;
        
        // 遍历所有活跃设备，单独上报每个设备（保持网关格式）
        for (int i = 0; i < 12; i++) {
            if (is_device_active[i]) {
                
                // 使用sprintf构建网关格式的JSON字符串
                static char json_buffer[512]; // 单设备JSON缓冲区，足够容纳一个设备的数据
                char temp_str[128], cell_str[256]; // 临时字符串缓冲区
                
                // 构建温度数组字符串
                snprintf(temp_str, sizeof(temp_str), "[%d,%d,%d,%d,%d]",
                        env_msg[i].temperature[0], env_msg[i].temperature[1],
                        env_msg[i].temperature[2], env_msg[i].temperature[3],
                        env_msg[i].temperature[4]);
                
                // 构建电池电压数组字符串
                snprintf(cell_str, sizeof(cell_str), 
                        "[%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d]",
                        env_msg[i].cell_voltages[0], env_msg[i].cell_voltages[1],
                        env_msg[i].cell_voltages[2], env_msg[i].cell_voltages[3],
                        env_msg[i].cell_voltages[4], env_msg[i].cell_voltages[5],
                        env_msg[i].cell_voltages[6], env_msg[i].cell_voltages[7],
                        env_msg[i].cell_voltages[8], env_msg[i].cell_voltages[9],
                        env_msg[i].cell_voltages[10], env_msg[i].cell_voltages[11]);
                
                // 构建完整的网关格式JSON（修改child字段格式）
                snprintf(json_buffer, sizeof(json_buffer),
                        "{\"devices\":[{\"device_id\":\"680b91649314d11851158e8d_Battery%02d\",\"services\":[{\"service_id\":\"ws63\","
                        "\"properties\":{\"temperature\":%s,\"current\":%d,\"total_voltage\":%d,"
                        "\"SOC\":%d,\"cell_voltages\":%s,\"level\":%d,\"child\":\"%s\"}}]}]}",  // child改为字符串格式
                        i, temp_str, 
                        env_msg[i].current, env_msg[i].total_voltage, env_msg[i].soc, cell_str,
                        env_msg[i].level, env_msg[i].child);  // 使用字符串格式
                
                char *json_str = json_buffer;
                
                printf("[4G] Publishing gateway device 680b91649314d11851158e8d_Battery%02d", i);
                
                if (gate_report_topic && json_str) {
                    L610_HuaweiCloudReport((char*)gate_report_topic, json_str);
                    published_count++;
                    printf("[L610] 网关设备 680b91649314d11851158e8d_Battery%02d 4G上报成功\r\n", i);
                    
                    // 设备间上报延时，避免L610模块负载过大
                    osal_msleep(200);
                } else {
                    printf("[L610] 网关设备 680b91649314d11851158e8d_Battery%02d JSON生成或发送失败\r\n", i);
                }
            }
        }
        
        printf("[L610] 4G网关单设备上报完成,成功上报设备数量:%d/%d\r\n", published_count, active_count);
        return published_count;
    } else {
        printf("[L610] 跳过数据上报:无活跃BMS设备连接\r\n");
        return 0;
    }
}

