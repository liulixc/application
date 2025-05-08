
/**
 * @defgroup SLE_Distribute_Network_Server
 * @ingroup  SLE_Distribute_Network
 * @brief    SLE分布式网络服务端头文件，定义了服务端相关的UUID和接口。
 * @{
 */
#ifndef SLE_DISTRIBUTE_NETWORK_SERVER_H
#define SLE_DISTRIBUTE_NETWORK_SERVER_H

#include "sle_ssap_server.h" // 包含SSAP服务端相关接口声明

/* SLE分布式网络服务端Service的UUID，用于唯一标识该BLE服务 */
#define SLE_UUID_SERVER_SERVICE 0xABCD

/* SLE分布式网络服务端通知（Notify）特征的UUID，用于上报数据给客户端 */
#define SLE_UUID_SERVER_NTF_REPORT 0x1122

/* SLE分布式网络服务端属性（Property）特征的UUID，用于配置或读取属性 */
#define SLE_UUID_SERVER_PROPERTY 0x3344

#endif

// SLE_UUID_SERVER_SERVICE、SLE_UUID_SERVER_NTF_REPORT 和 SLE_UUID_SERVER_PROPERTY 这几个 UUID 只写了四位（如 0xABCD），是因为它们采用了16位（2字节）UUID 格式。

// 在 BLE（蓝牙低功耗）协议中，UUID 有三种长度：

// 16位（如 0xABCD）：用于标准或自定义的短 UUID，实际使用时会自动扩展为 128位 UUID，格式为 0000xxxx-0000-1000-8000-00805F9B34FB，其中 xxxx 就是你定义的 16位值。
// 32位：较少用。
// 128位：完整自定义 UUID。
// 所以这里只写四位（16位 UUID），是为了简化 BLE 服务和特征的定义，实际协议栈会自动转换为完整的 128位 UUID。如果需要全局唯一性或和标准服务区分，建议使用 128位自定义 UUID。