/**
 * @file WiFi_STA.h
 * @brief WiFi STA模式功能接口头文件
 *
 * 本文件声明了WiFi STA模式下查找并连接指定WiFi热点的接口函数。
 * 适用于分布式网络客户端，通过该接口可实现WiFi自动扫描、连接及DHCP获取IP等功能。
 */

/**
 * @defgroup WiFi_STA_Module WiFi STA模块
 * @ingroup  SLE_Distribute_Network_Client
 * @brief    提供WiFi STA模式相关功能接口
 * @{
 */

#ifndef WIFI_STA_H
#define WIFI_STA_H

/**
 * @brief  启动WiFi STA功能，查找并连接WiFi热点。
 *
 * 该函数会自动扫描指定SSID的WiFi热点，并尝试连接，连接成功后通过DHCP获取IP地址。
 *
 * @param  [in] ssid      WiFi热点的SSID字符串指针
 * @param  [in] ssid_len  SSID的长度（包括字符串结尾的'\0'）
 * @param  [in] key       WiFi热点的密码字符串指针
 * @param  [in] key_len   密码的长度（包括字符串结尾的'\0'）
 * @retval ERRCODE_SLE_SUCCESS    执行成功，已连接并获取到IP
 * @retval ERRCODE_SLE_FAIL       执行失败，未能连接或获取IP
 *
 * @note   依赖底层WiFi驱动和DHCP协议栈，调用前需确保相关模块已初始化。
 */
errcode_t example_sta_function(const char *ssid, uint8_t ssid_len, const char *key, uint8_t key_len);

#endif