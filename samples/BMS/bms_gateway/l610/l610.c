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

// 外部声明OTA上下文
extern l610_ota_context_t g_l610_ota_ctx;
extern uint32_t parsed_file_size;


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

// 全局变量定义
static void* l610_mutex = NULL; // osal_mutex_t未定义时用void*
static uint8_t l610_ready_flag = 0; // L610模块就绪标志位: 0-未就绪, 1-已就绪



uint8_t isPrintf=1;	//定义于main函数: 是否打印日志

#define CMD_LEN 1024
char cmdSend[CMD_LEN];		//发送上报数据的AT指令
uint32_t DefaultTimeout=1000;//超时


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
    // printf("[L610] Received: %.*s\r\n", copy_len, (char*)uart2_recv.recv);
    
    // 检查多种可能的就绪状态，重点匹配"+SIM READY"
    if (strstr((char*)uart2_recv.recv, "READY") ) {
        // SIM卡准备好后发送AT指令激活模块
        printf("[L610] Sending AT command to activate module\r\n");
        uart_send_buff((uint8_t *)"AT\r\n", 4);
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
    uart_buffer_config.rx_buffer_size = 1024;
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

bool first =1;
void L610_Attach(uint8_t isPrintf,uint8_t isReboot) {
	// 检查模块是否已初始化

	if (!l610_ready_flag) {
		printf("[L610] Waiting for module initialization...\r\n");
		// 等待最多5秒钟模块初始化完成
		if (!L610_WaitForInit(10000)) {
			printf("[L610] Module initialization timeout, attempting attach anyway\r\n");
		}
        l610_ready_flag=1;
	}
	
	if (isReboot== 1) {
        if(first){
            printf("wait 3秒");
            osal_msleep(3000);
            first=0;
        }
        
        // 关闭回显
        L610_SendCmd("ATE0\r\n", "OK", 1000, isPrintf);
        if (isPrintf) printf("[L610] Echo disabled\r\n");
        
		L610_SendCmd((uint8_t *) "AT+MIPCALL=1\r\n", (uint8_t *) "OK", 1000,isPrintf);
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
    if (!ip || !port || !clientid || !password) {
        printf("[L610] HuaweiCloudConnect: Invalid parameters\r\n");
        return;
    }
    
    memset(cmdSend, 0, sizeof(cmdSend));
    snprintf(cmdSend, sizeof(cmdSend),
        "AT+HMCON=0,%d,\"%s\",\"%s\",\"%s\",\"%s\",%d\r\n",
        keepalive, ip, port, clientid, password, cleanSession);
    L610_SendCmd(cmdSend, "OK", 5000, isPrintf);
}

/*
函数名称: L610_HuaweiCloudSubscribe
说明: 华为云平台MQTT订阅自定义主题
参数: 
    int qos: 主题的QoS等级 (0, 1, 2)
    char *topic: 自定义主题字符串
    uint8_t isPrintf: 是否打印日志
返回值: 无
*/
void L610_HuaweiCloudSubscribe(int qos, char *topic, uint8_t isPrintf) {
    if (topic == NULL) {
        if (isPrintf) printf("[L610] Subscribe topic is NULL!\r\n");
        return;
    }
    
    memset(cmdSend, 0, sizeof(cmdSend));
    snprintf(cmdSend, sizeof(cmdSend), "AT+HMSUB=%d,\"%s\"\r\n", qos, topic);
    
    if (isPrintf) {
        printf("[L610] Subscribing to topic: %s with QoS: %d\r\n", topic, qos);
    }
    
    L610_SendCmd(cmdSend, "OK", DefaultTimeout, isPrintf);
}

/**
 * @brief 从MQTT命令主题中提取request_id
 * @param topic MQTT主题字符串，格式如："$oc/devices/680b91649314d11851158e8d_Battery01/sys/commands/request_id=a8c3df05-2003-4445-8d2b-3130fec1772e"
 * @return 提取的request_id字符串，需要调用者释放内存；失败返回NULL
 */
char* l610_ota_extract_request_id(const char* topic)
{
    if (!topic) return NULL;
    
    // 查找"request_id="字符串
    const char* request_id_start = strstr(topic, "request_id=");
    if (!request_id_start) {
        osal_printk("[L610 OTA]: request_id not found in topic: %s\r\n", topic);
        return NULL;
    }
    
    // 跳过"request_id="
    request_id_start += 11;
    
    // 查找request_id的结束位置（遇到逗号、引号或字符串结束）
    const char* request_id_end = request_id_start;
    while (*request_id_end && *request_id_end != ',' && *request_id_end != '"' && *request_id_end != '\r' && *request_id_end != '\n') {
        request_id_end++;
    }
    
    // 计算request_id长度
    int request_id_len = request_id_end - request_id_start;
    if (request_id_len <= 0 || request_id_len > 64) {
        osal_printk("[L610 OTA]: Invalid request_id length: %d\r\n", request_id_len);
        return NULL;
    }
    
    // 分配内存并复制request_id
    char* request_id = (char*)malloc(request_id_len + 1);
    if (!request_id) {
        osal_printk("[L610 OTA]: Failed to allocate memory for request_id\r\n");
        return NULL;
    }
    
    memcpy(request_id, request_id_start, request_id_len);
    request_id[request_id_len] = '\0';
    
    osal_printk("[L610 OTA]: Extracted request_id: %s\r\n", request_id);
    return request_id;
}

/**
 * @brief 处理升级命令
 * @param payload MQTT消息载荷
 * @return 0-成功, -1-失败
 */
int l610_ota_process_upgrade_command(const char* payload)
{
    if (!payload) return -1;
    
    osal_printk("[L610 OTA]: Processing upgrade command: %s\r\n", payload);
    
    // 解析JSON格式的OTA升级命令
    char* command_name = strstr(payload, "\"command_name\":");
    if (command_name && strstr(command_name, "\"ota_upgrade\"")) {
        // 解析firmware_path
        char* firmware_path_start = strstr(payload, "\"firmware_path\":");
        if (firmware_path_start) {
            osal_printk("[L610 OTA]: Found firmware_path field\r\n");
            
            // 找到冒号后的第一个引号
            char* colon_pos = strchr(firmware_path_start, ':');
            if (colon_pos) {
                char* first_quote = strchr(colon_pos, '"');
                if (first_quote) {
                    // 跳过第一个引号，指向路径内容的开始
                    char* path_start = first_quote + 1;
                    // 找到路径结束的引号
                    char* path_end = strchr(path_start, '"');
                    if (path_end) {
                        int path_len = path_end - path_start;
                        if (path_len > 0 && path_len < 256) {
                            // 打印调试信息
                            osal_printk("[L610 OTA]: Extracted path length: %d\r\n", path_len);
                            osal_printk("[L610 OTA]: Path content: %.*s\r\n", path_len, path_start);
                            
                            // 构建完整的固件URL
                            snprintf(g_l610_ota_ctx.firmware_url, sizeof(g_l610_ota_ctx.firmware_url), 
                                    "http://1.13.92.135:7998%.*s", path_len, path_start);
                            osal_printk("[L610 OTA]: Firmware URL set to: %s\r\n", g_l610_ota_ctx.firmware_url);
                        } else {
                            osal_printk("[L610 OTA]: Invalid path length: %d\r\n", path_len);
                        }
                    } else {
                        osal_printk("[L610 OTA]: Path end quote not found\r\n");
                    }
                } else {
                    osal_printk("[L610 OTA]: First quote after colon not found\r\n");
                }
            } else {
                osal_printk("[L610 OTA]: Colon after firmware_path not found\r\n");
            }
        } else {
            osal_printk("[L610 OTA]: firmware_path field not found in payload\r\n");
        }
        
        // 4G模块只处理网关升级，不需要解析device_id
         osal_printk("[L610 OTA]: Target device is gateway (4G module)\r\n");
        
        // 解析size
         char* size_start = strstr(payload, "\"size\":");
         if (size_start) {
             size_start = strchr(size_start, ':');
             if (size_start) {
                 size_start++; // 跳过冒号
                 while (*size_start == ' ' || *size_start == '\t') size_start++; // 跳过空格
                 uint32_t file_size = (uint32_t)atoi(size_start);
                 osal_printk("[L610 OTA]: Firmware size: %u bytes\r\n", file_size);
                 
                 // 将解析出的文件大小设置到全局变量
                 parsed_file_size = file_size;
                 osal_printk("[L610 OTA]: File size set to: %u bytes\r\n", parsed_file_size);
             }
         }
        
        osal_printk("[L610 OTA]: OTA upgrade command parsed successfully\r\n");
        return 0;
    }
    
    // 兼容旧的简单命令格式
    if (strstr(payload, "upgrade") || strstr(payload, "ota")) {
        osal_printk("[L610 OTA]: Legacy upgrade command detected\r\n");
        return 0;
    }
    
    return -1;
}



/**
 * @brief 处理升级命令
 * @param command_data 命令数据
 */
void l610_ota_handle_upgrade_command(char *command_data)
{
    if (!command_data) return;
    
    osal_printk("[L610 OTA]: Handling upgrade command: %s\r\n", command_data);
    
    if (l610_ota_process_upgrade_command(command_data) == 0) {
        // 从命令数据中提取request_id
        char* request_id = l610_ota_extract_request_id(command_data);
        if (request_id) {
            // 回复确认消息，使用提取的request_id
            char reply_topic[128];
            snprintf(reply_topic, sizeof(reply_topic), 
                     MQTT_CLIENT_RESPONSE, 
                     request_id);
            L610_HuaweiCloudReport(reply_topic, "{\"result_code\":0,\"response_name\":\"OTA_START\"}");
            
            // 释放request_id内存
            free(request_id);
        }
    }
}


/********************4G OTA升级****************************/

/********************HTTP协议****************************/

/*
函数名称: L610_HttpSetUrl
说明: 设置HTTP URL参数
参数: 
    char *url: HTTP请求的URL地址
    uint8_t isPrintf: 是否打印日志
返回值: 无
*/
void L610_HttpSetUrl(char *url, uint8_t isPrintf) {
    if (url == NULL) {
        if (isPrintf) printf("[L610] HTTP URL is NULL!\r\n");
        return;
    }
    
    memset(cmdSend, 0, sizeof(cmdSend));
    snprintf(cmdSend, sizeof(cmdSend), "AT+HTTPSET=\"URL\",\"%s\"\r\n", url);
    
    if (isPrintf) {
        printf("[L610] Setting HTTP URL: %s\r\n", url);
    }
    
    L610_SendCmd(cmdSend, "OK", DefaultTimeout, isPrintf);
}

/*
函数名称: L610_HttpSetUserAgent
说明: 设置HTTP User-Agent参数
参数: 
    char *user_agent: HTTP User-Agent字符串
    uint8_t isPrintf: 是否打印日志
返回值: 无
*/
void L610_HttpSetUserAgent(char *user_agent, uint8_t isPrintf) {
    if (user_agent == NULL) {
        if (isPrintf) printf("[L610] HTTP User-Agent is NULL!\r\n");
        return;
    }
    
    memset(cmdSend, 0, sizeof(cmdSend));
    snprintf(cmdSend, sizeof(cmdSend), "AT+HTTPSET=\"UAGENT\",\"%s\"\r\n", user_agent);
    
    if (isPrintf) {
        printf("[L610] Setting HTTP User-Agent: %s\r\n", user_agent);
    }
    
    L610_SendCmd(cmdSend, "OK", DefaultTimeout, isPrintf);
}

/*
函数名称: L610_HttpSetRange
说明: 设置HTTP Range参数
参数: 
    char *range: HTTP Range字符串
    uint8_t isPrintf: 是否打印日志
返回值: 无
*/
void L610_HttpSetRange(char *range, uint8_t isPrintf) {
    if (range == NULL) {
        if (isPrintf) printf("[L610] HTTP Range is NULL!\r\n");
        return;
    }
    
    memset(cmdSend, 0, sizeof(cmdSend));
    snprintf(cmdSend, sizeof(cmdSend), "AT+HTTPSET=\"RANGE\",\"%s\"\r\n", range);
    
    if (isPrintf) {
        printf("[L610] Setting HTTP Range: %s\r\n", range);
    }
    
    L610_SendCmd(cmdSend, "OK", DefaultTimeout, isPrintf);
}

/*
函数名称: L610_HttpAction
说明: 开始HTTP业务操作
参数: 
    int method: HTTP方法 (0=GET, 1=POST, 2=HEAD)
    uint8_t isPrintf: 是否打印日志
返回值: 无
*/
void L610_HttpAction(int method, uint8_t isPrintf) {
    memset(cmdSend, 0, sizeof(cmdSend));
    snprintf(cmdSend, sizeof(cmdSend), "AT+HTTPACT=%d\r\n", method);
    
    if (isPrintf) {
        const char *method_str[] = {"GET", "POST", "HEAD"};
        if (method >= 0 && method <= 2) {
            printf("[L610] Starting HTTP %s request\r\n", method_str[method]);
        } else {
            printf("[L610] Starting HTTP request with method: %d\r\n", method);
        }
    }
    
    // HTTP操作可能需要更长的超时时间
    L610_SendCmd(cmdSend, "+HTTPRES:", 5000, isPrintf);

}

/*
函数名称: L610_HttpRead
说明: 从模块中读取HTTP响应数据
参数: 
    int offset: 读取数据的偏移量
    int length: 读取数据的长度 (0表示读取全部)
    uint8_t isPrintf: 是否打印日志
返回值: 无
*/
void L610_HttpRead(int offset, int length, uint8_t isPrintf) {
    memset(cmdSend, 0, sizeof(cmdSend));
    
    if (length > 0) {
        // 指定偏移量和长度
        snprintf(cmdSend, sizeof(cmdSend), "AT+HTTPREAD=%d,%d\r\n", offset, length);
        if (isPrintf) {
            printf("[L610] Reading HTTP data: offset=%d, length=%d\r\n", offset, length);
        }
    } else {
        // 读取全部数据
        snprintf(cmdSend, sizeof(cmdSend), "AT+HTTPREAD\r\n");
        if (isPrintf) {
            printf("[L610] Reading all HTTP data\r\n");
        }
    }
    
    // HTTP读取可能需要较长的超时时间
    uart2_recv.recv_flag = 0;
    uart2_recv.recv_len = 0;
    memset(uart2_recv.recv, 0, sizeof(uart2_recv.recv));
    uart_send_buff((uint8_t *)cmdSend, strlen(cmdSend));
}

/*
函数名称: L610_HttpGet
说明: 执行完整的HTTP GET请求
参数: 
    char *url: 请求的URL地址
    char *user_agent: User-Agent字符串 (可为NULL使用默认值)
    uint8_t isPrintf: 是否打印日志
返回值: 无
*/
void L610_HttpGet(char *url, char *user_agent, uint8_t isPrintf) {
    if (url == NULL) {
        if (isPrintf) printf("[L610] HTTP GET: URL is NULL!\r\n");
        return;
    }
    
    if (isPrintf) {
        printf("[L610] Starting HTTP GET request to: %s\r\n", url);
    }
    
    // 1. 设置URL
    L610_HttpSetUrl(url, isPrintf);
    
    // 2. 设置User-Agent (如果提供)
    if (user_agent != NULL) {
        L610_HttpSetUserAgent(user_agent, isPrintf);
    }
    
    // 3. 执行GET请求
    L610_HttpAction(0, isPrintf);
    
    if (isPrintf) {
        printf("[L610] HTTP GET request initiated.\r\n");
    }
}

/*
函数名称: L610_HttpGetWithRange
说明: 执行带Range的HTTP GET请求
参数: 
    char *url: 请求的URL地址
    char *range: HTTP Range字符串
    uint8_t isPrintf: 是否打印日志
返回值: 无
*/
void L610_HttpGetWithRange(char *url, char *range, uint8_t isPrintf) {
    if (url == NULL) {
        if (isPrintf) printf("[L610] HTTP GET with Range: URL is NULL!\r\n");
        return;
    }
    
    if (isPrintf) {
        printf("[L610] Starting HTTP GET with Range request to: %s, Range: %s\r\n", url, range ? range : "NULL");
    }
    
    // 1. 设置URL
    L610_HttpSetUrl(url, isPrintf);
    
    // 2. 设置Range (如果提供)
    if (range != NULL && strlen(range) > 0) {
        L610_HttpSetRange(range, isPrintf);
    }
    
    // 3. 设置默认User-Agent
    L610_HttpSetUserAgent("fibocom", isPrintf);
    
    // 4. 执行GET请求
    L610_HttpAction(0, isPrintf);
    
    if (isPrintf) {
        printf("[L610] HTTP GET with Range request initiated\r\n");
    }
}

// 华为云平台上报数据
void L610_HuaweiCloudReport(char *topic, char *payload) {
    if (topic == NULL || payload == NULL) {
        printf("[L610] Report: topic or payload is NULL!\r\n");
        return;
    }
    
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

