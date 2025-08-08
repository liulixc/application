#include "string.h"
#include "stdlib.h"
#include "uart.h"
#include "soc_osal.h"
#include "app_init.h"
#include "cmsis_os2.h"
#include "unistd.h"
#include "upg.h"
#include "partition.h"
#include "upg_porting.h"
#include "gpio.h"
#include "pinctrl.h"
#include "osal_addr.h"
#include "osal_wait.h"
#include "securec.h"

#define UART_TASK_STACK_SIZE 0x2000
#define RECV_BUFFER_SIZE     1024

// 串口配置 - 参考l610配置
#define UART_ID UART_BUS_2
#define UART_RX_MAX 1024
#define CONFIG_UART_TXD_PIN 8 
#define CONFIG_UART_RXD_PIN 7
#define CONFIG_UART_PIN_MODE 2

// 简单的UART OTA升级
// 协议格式: [文件大小4字节][固件数据]

typedef struct {
    uint8_t recv[UART_RX_MAX];
    uint16_t recv_len;
    uint8_t recv_flag;
} uart_recv_t;

// 全局变量
uint8_t uart_rx_buffer[UART_RX_MAX];
uart_recv_t uart_recv = {0};
static uint32_t total_file_size = 0;
static uint32_t received_size = 0;
static uint8_t ota_started = 0;

// 函数声明
void uart_ota_init_config(void);
void uart_read_handler(const void *buffer, uint16_t length, bool error);
errcode_t ota_prepare(uint32_t file_size);



// OTA准备函数 - 复用原有逻辑
errcode_t ota_prepare(uint32_t file_size)
{
    osal_printk("[oat task]:get in ota_task \r\n");
    uint32_t max_len;
    errcode_t ret = 0;
    
    // 依赖分区模块
    ret = uapi_partition_init();
    if (ret != 0) {
        osal_printk("[UART OTA]: partition init error\r\n");
        return -1;
    }
    
    // 获取APP升级文件大小上限
    max_len = uapi_upg_get_storage_size();
    
    osal_printk("[UART OTA]: Available storage size: %u bytes\r\n", max_len);
    osal_printk("[UART OTA]: File size: %u bytes\r\n", file_size);
    
    if (file_size > max_len) {
        osal_printk("[UART OTA]: file size too large (file: %u > max: %u)\r\n", file_size, max_len);
        return -1;
    }
    
    upg_prepare_info_t upg_prepare_info;
    upg_prepare_info.package_len = file_size;
    
    ret = uapi_upg_prepare(&upg_prepare_info);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[UART OTA]: prepare failed\r\n");
        return -1;
    }
    
    return ERRCODE_SUCC;
}



// 串口接收回调
void uart_read_handler(const void *buffer, uint16_t length, bool error)
{
    unused(error);
    
    if (length > 0 && length <= sizeof(uart_recv.recv)) {
        memset(uart_recv.recv, 0, sizeof(uart_recv.recv));
        memcpy_s(uart_recv.recv, sizeof(uart_recv.recv), buffer, length);
        uart_recv.recv_len = length;
        uart_recv.recv_flag = 1;
    }
}

// 串口初始化配置 - 参考l610配置
void uart_ota_init_config(void)
{
    uart_buffer_config_t uart_buffer_config;
    
    // 配置引脚
    uapi_pin_set_mode(CONFIG_UART_TXD_PIN, CONFIG_UART_PIN_MODE);
    uapi_pin_set_mode(CONFIG_UART_RXD_PIN, CONFIG_UART_PIN_MODE);
    
    // 串口属性配置
    uart_attr_t attr = {
        .baud_rate = 115200, //115200传输有问题
        .data_bits = UART_DATA_BIT_8, 
        .stop_bits = UART_STOP_BIT_1, 
        .parity = UART_PARITY_NONE
    };
    
    // 缓冲区配置
    uart_buffer_config.rx_buffer_size = UART_RX_MAX;
    uart_buffer_config.rx_buffer = uart_rx_buffer;
    
    // 引脚配置
    uart_pin_config_t pin_config = {
        .tx_pin = S_MGPIO0, 
        .rx_pin = S_MGPIO1, 
        .cts_pin = PIN_NONE, 
        .rts_pin = PIN_NONE
    };
    
    // 初始化串口
    uapi_uart_deinit(UART_ID);
    int res = uapi_uart_init(UART_ID, &pin_config, &attr, NULL, &uart_buffer_config);
    if (res != 0) {
        osal_printk("[UART OTA]: uart init failed res = %02x\r\n", res);
        return;
    }
    
    // 注册接收回调
    if (uapi_uart_register_rx_callback(UART_ID, UART_RX_CONDITION_MASK_IDLE, 1, uart_read_handler) == ERRCODE_SUCC) {
        osal_printk("[UART OTA]: uart%d register receive callback success!\r\n", UART_ID);
    }
    
    osal_printk("[UART OTA]: UART OTA initialized successfully\r\n");
}

// UART OTA主任务
static void uart_ota_task(void *argument)
{
    unused(argument);
    errcode_t ret;
    
    osal_printk("[UART OTA]: UART OTA task started\r\n");
    
    // 初始化串口
    uart_ota_init_config();
    
    osal_printk("[UART OTA]: Ready for OTA upgrade\r\n");
    osal_printk("[UART OTA]: Send file size (4 bytes) + firmware data\r\n");
    
    // 主循环 - 处理接收到的数据
    while (1) {
        if (uart_recv.recv_flag) {
            uart_recv.recv_flag = 0;
            
            // 如果还没开始OTA，先接收文件大小
            if (!ota_started && uart_recv.recv_len >= 4) {
                // 解析文件大小 (小端序)
                total_file_size = (uint32_t)uart_recv.recv[0] | 
                                 ((uint32_t)uart_recv.recv[1] << 8) |
                                 ((uint32_t)uart_recv.recv[2] << 16) |
                                 ((uint32_t)uart_recv.recv[3] << 24);
                
                osal_printk("[UART OTA]: File size: %d bytes\r\n", total_file_size);
                
                if (ota_prepare(total_file_size) == ERRCODE_SUCC) {
                    received_size = 0;
                    ota_started = 1;
                    osal_printk("[UART OTA]: OTA started, send firmware data now\r\n");
                    
                    // 如果第一个包中包含固件数据，先写入
                    if (uart_recv.recv_len > 4) {
                        uint16_t data_len = uart_recv.recv_len - 4;
                        ret = uapi_upg_write_package_sync(0, &uart_recv.recv[4], data_len);
                        if (ret == ERRCODE_SUCC) {
                            received_size = data_len;
                            osal_printk("[UART OTA]: Written %d bytes\r\n", data_len);
                        }
                    }
                } else {
                    osal_printk("[UART OTA]: OTA prepare failed\r\n");
                }
            }
            // 如果已经开始OTA，接收固件数据
            else if (ota_started && uart_recv.recv_len > 0) {
                // 计算本次需写入的字节数（防止超量）
                int write_size = uart_recv.recv_len;
                if (received_size + uart_recv.recv_len > total_file_size) {
                    write_size = total_file_size - received_size;
                    osal_printk("[UART OTA]: adjusting write_size from %d to %d\r\n", uart_recv.recv_len, write_size);
                }
                
                if (write_size > 0) {
                    ret = uapi_upg_write_package_sync(received_size, uart_recv.recv, write_size);
                    if (ret == ERRCODE_SUCC) {
                        received_size += write_size;
                        
                        // 添加数据完整性检查（参考HTTP OTA）
                        if (received_size % 4096 == 0 || received_size == total_file_size) {
                            int progress_percent = (received_size * 100) / total_file_size;
                            osal_printk("[UART OTA]: progress checkpoint: %d/%d (%d%%)\r\n", 
                                       received_size, total_file_size, progress_percent);
                        }
                        
                        // 超量保护（防止循环条件失效）
                        if (received_size > total_file_size) {
                            received_size = total_file_size;
                        }
                        
                        // 检查是否接收完成
                        if (received_size >= total_file_size) {
                            osal_printk("[UART OTA]: recv all succ\r\n");
                            
                            osal_printk("[UART OTA]: data integrity check passed, starting upgrade...\r\n");
                            
                            ret = uapi_upg_request_upgrade(false);
                            if (ret == ERRCODE_SUCC) {
                                osal_printk("[UART OTA]: upgrade request successful, system will reboot...\r\n");
                                osal_msleep(1000);
                                upg_reboot();
                            } else {
                                osal_printk("[UART OTA]: uapi_upg_request_upgrade error = 0x%x\r\n", ret);
                            }
                        }
                    } else {
                        osal_printk("[UART OTA]: Write failed, ret = 0x%x\r\n", ret);
                    }
                } else {
                    osal_printk("[UART OTA]: write_size <= 0, skipping\r\n");
                }
            }
            
        }
        // 喂狗
        upg_watchdog_kick();
    }
}

// 创建UART OTA任务
static void uart_ota_sample(void)
{
    osThreadAttr_t attr;
    attr.name = "uart_ota_task";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = UART_TASK_STACK_SIZE;
    attr.priority = osPriorityNormal;
    
    if (osThreadNew((osThreadFunc_t)uart_ota_task, NULL, &attr) == NULL) {
        osal_printk("[UART OTA]: Create uart_ota_task fail.\r\n");
    } else {
        osal_printk("[UART OTA]: Create uart_ota_task success.\r\n");
    }
}

app_run(uart_ota_sample);