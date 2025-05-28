/*
 * Copyright (c) 2024 HiSilicon Technologies CO., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * @file wifi_udp_sample.c
 * @brief Wi-Fi STA模式UDP通信样例主流程实现文件
 *
 * 本文件实现了Wi-Fi STA模式下的UDP通信流程，包括Wi-Fi连接、DHCP获取IP、
 * 以及UDP socket的创建、数据收发等。适用于HiSilicon平台的Wi-Fi UDP通信样例。
 *
 * 主要流程：
 * 1. 注册Wi-Fi事件回调，等待Wi-Fi初始化完成。
 * 2. 启动扫描，筛选目标AP，连接Wi-Fi并获取IP。
 * 3. 创建UDP socket，向指定服务器周期性发送数据并接收响应。
 *
 * 主要函数说明：
 * - wifi_scan_state_changed：扫描完成事件回调。
 * - wifi_connection_changed：连接状态变化事件回调。
 * - example_get_match_network：筛选目标AP并填充连接参数。
 * - wifi_connect：Wi-Fi连接主流程。
 * - sta_sample_init：样例主入口，完成Wi-Fi连接和UDP通信。
 * - sta_sample：创建样例任务线程。
 */

#include "lwip/netifapi.h"
#include "wifi_hotspot.h"
#include "wifi_hotspot_config.h"
#include "stdlib.h"
#include "uart.h"
#include "lwip/nettool/misc.h"
#include "soc_osal.h"
#include "app_init.h"
#include "cmsis_os2.h"
#include "wifi_device.h"
#include "wifi_event.h"
#include "lwip/sockets.h"
#include "lwip/ip4_addr.h"

#define WIFI_TASK_STACK_SIZE 0x2000
#define WIFI_SCAN_AP_LIMIT 64
#define WIFI_CONN_STATUS_MAX_GET_TIMES 5 /* 启动连接之后，判断是否连接成功的最大尝试次数 */
#define DHCP_BOUND_STATUS_MAX_GET_TIMES 20 /* 启动DHCP Client端功能之后，判断是否绑定成功的最大尝试次数 */
#define WIFI_STA_IP_MAX_GET_TIMES 5 /* 判断是否获取到IP的最大尝试次数 */

#define CONFIG_WIFI_SSID "QQ"      // 要连接的WiFi 热点账号
#define CONFIG_WIFI_PWD "tangyuan"        // 要连接的WiFi 热点密码
#define CONFIG_SERVER_IP "192.168.84.24" // 要连接的服务器IP
#define CONFIG_SERVER_PORT 8888            // 要连接的服务器端口

static const char *SEND_DATA = "UDP Test!\r\n";

/**
 * @brief STA扫描事件回调函数
 * @param state 扫描状态
 * @param size 扫描到的AP数量
 * @note 扫描完成后打印提示
 */
static void wifi_scan_state_changed(td_s32 state, td_s32 size)
{
    UNUSED(state);
    UNUSED(size);
    printf("Scan done!\r\n");
    return;
}

/**
 * @brief STA连接状态事件回调函数
 * @param state 连接状态
 * @param info 连接信息
 * @param reason_code 失败原因码
 * @note 连接成功时打印SSID和信号强度
 */
static void wifi_connection_changed(td_s32 state, const wifi_linked_info_stru *info, td_s32 reason_code)
{
    UNUSED(reason_code);

    if (state == WIFI_STATE_AVALIABLE)
        printf("[WiFi]:%s, [RSSI]:%d\r\n", info->ssid, info->rssi);
}
/*****************************************************************************
  STA 扫描-关联 sample用例
*****************************************************************************/

/**
 * @brief 筛选扫描到的目标AP并填充连接参数
 * @param expected_ssid 期望连接的AP SSID
 * @param key 连接密码
 * @param expected_bss 期望连接的AP参数结构体
 * @return ERRCODE_SUCC: 成功, 其他: 失败
 * @note 先扫描所有AP，找到目标后填充expected_bss
 */
static errcode_t example_get_match_network(const char *expected_ssid,
                                           const char *key,
                                           wifi_sta_config_stru *expected_bss)
{
    uint32_t num = WIFI_SCAN_AP_LIMIT; /* 64:扫描到的Wi-Fi网络数量 */
    uint32_t bss_index = 0;

    /* 获取扫描结果 */
    uint32_t scan_len = sizeof(wifi_scan_info_stru) * WIFI_SCAN_AP_LIMIT;
    wifi_scan_info_stru *result = osal_kmalloc(scan_len, OSAL_GFP_ATOMIC);
    if (result == NULL) {
        return ERRCODE_MALLOC;
    }

    memset_s(result, scan_len, 0, scan_len);
    if (wifi_sta_get_scan_info(result, &num) != ERRCODE_SUCC) {
        osal_kfree(result);
        return ERRCODE_FAIL;
    }

    /* 筛选扫描到的Wi-Fi网络，选择待连接的网络 */
    for (bss_index = 0; bss_index < num; bss_index++) {
        if (strlen(expected_ssid) == strlen(result[bss_index].ssid)) {
            if (memcmp(expected_ssid, result[bss_index].ssid, strlen(expected_ssid)) == 0) {
                break;
            }
        }
    }

    /* 未找到待连接AP,可以继续尝试扫描或者退出 */
    if (bss_index >= num) {
        osal_kfree(result);
        return ERRCODE_FAIL;
    }
    /* 找到网络后复制网络信息和接入密码 */
    if (memcpy_s(expected_bss->ssid, WIFI_MAX_SSID_LEN, result[bss_index].ssid, WIFI_MAX_SSID_LEN) != EOK) {
        osal_kfree(result);
        return ERRCODE_MEMCPY;
    }
    if (memcpy_s(expected_bss->bssid, WIFI_MAC_LEN, result[bss_index].bssid, WIFI_MAC_LEN) != EOK) {
        osal_kfree(result);
        return ERRCODE_MEMCPY;
    }
    expected_bss->security_type = result[bss_index].security_type;
    if (memcpy_s(expected_bss->pre_shared_key, WIFI_MAX_KEY_LEN, key, strlen(key)) != EOK) {
        osal_kfree(result);
        return ERRCODE_MEMCPY;
    }
    expected_bss->ip_type = DHCP; /* IP类型为动态DHCP获取 */
    osal_kfree(result);
    return ERRCODE_SUCC;
}

/**
 * @brief Wi-Fi连接主流程
 * @return ERRCODE_SUCC: 成功, 其他: 失败
 * @note 包含扫描、连接、DHCP获取IP等完整流程
 */
static errcode_t wifi_connect(void)
{
    char ifname[WIFI_IFNAME_MAX_SIZE + 1] = "wlan0"; /* WiFi STA 网络设备名 */
    wifi_sta_config_stru expected_bss = {0};         /* 连接请求信息 */
    const char expected_ssid[] = CONFIG_WIFI_SSID;
    const char key[] = CONFIG_WIFI_PWD; /* 待连接的网络接入密码 */
    struct netif *netif_p = NULL;
    wifi_linked_info_stru wifi_status;
    uint8_t index = 0;

    /* 创建STA */
    if (wifi_sta_enable() != ERRCODE_SUCC) {
        printf("STA enbale fail !\r\n");
        return ERRCODE_FAIL;
    }
    do {
        printf("Start Scan !\r\n");
        (void)osDelay(100); /* 100: 延时1s  */
        /* 启动STA扫描 */
        if (wifi_sta_scan() != ERRCODE_SUCC) {
            printf("STA scan fail, try again !\r\n");
            continue;
        }

        (void)osDelay(300); /* 300: 延时3s  */

        /* 获取待连接的网络 */
        if (example_get_match_network(expected_ssid, key, &expected_bss) != ERRCODE_SUCC) {
            printf("Can not find AP, try again !\r\n");
            continue;
        }

        printf("STA start connect.\r\n");
        /* 启动连接 */
        if (wifi_sta_connect(&expected_bss) != ERRCODE_SUCC) {
            continue;
        }

        /* 检查网络是否连接成功 */
        for (index = 0; index < WIFI_CONN_STATUS_MAX_GET_TIMES; index++) {
            (void)osDelay(50); /* 50： 延时500ms */
            memset_s(&wifi_status, sizeof(wifi_linked_info_stru), 0, sizeof(wifi_linked_info_stru));
            if (wifi_sta_get_ap_info(&wifi_status) != ERRCODE_SUCC) {
                continue;
            }
            if (wifi_status.conn_state == WIFI_CONNECTED) {
                break;
            }
        }
        if (wifi_status.conn_state == WIFI_CONNECTED) {
            break; /* 连接成功退出循环 */
        }
    } while (1);

    /* DHCP获取IP地址 */
    netif_p = netifapi_netif_find(ifname);
    if (netif_p == NULL) {
        return ERRCODE_FAIL;
    }

    if (netifapi_dhcp_start(netif_p) != ERR_OK) {
        printf("STA DHCP Fail.\r\n");
        return ERRCODE_FAIL;
    }

    for (uint8_t i = 0; i < DHCP_BOUND_STATUS_MAX_GET_TIMES; i++) {
        (void)osDelay(50); /* 延时500ms */
        if (netifapi_dhcp_is_bound(netif_p) == ERR_OK) {
            printf("STA DHCP bound success.\r\n");
            break;
        }
    }

    for (uint8_t i = 0; i < WIFI_STA_IP_MAX_GET_TIMES; i++) {
        osDelay(1); /* 1: 延时10ms  */
        if (netif_p->ip_addr.u_addr.ip4.addr != 0) {
            printf("STA IP %u.%u.%u.%u\r\n", (netif_p->ip_addr.u_addr.ip4.addr & 0x000000ff),
                   (netif_p->ip_addr.u_addr.ip4.addr & 0x0000ff00) >> 8,   /* 8: 移位  */
                   (netif_p->ip_addr.u_addr.ip4.addr & 0x00ff0000) >> 16,  /* 16: 移位  */
                   (netif_p->ip_addr.u_addr.ip4.addr & 0xff000000) >> 24); /* 24: 移位  */

            /* 连接成功 */
            printf("STA connect success.\r\n");
            return ERRCODE_SUCC;
        }
    }
    printf("STA connect fail.\r\n");
    return ERRCODE_FAIL;
}

/**
 * @brief 样例主入口，完成Wi-Fi连接和UDP通信
 * @param argument 任务参数（未使用）
 * @return 0: 成功, -1: 失败
 * @note 连接Wi-Fi后，创建UDP socket并循环收发数据
 */
int sta_sample_init(const char *argument)
{
    argument = argument;
    int sock_fd;
    // 服务器的地址信息
    struct sockaddr_in send_addr;
    socklen_t addr_length = sizeof(send_addr);
    char recv_buf[512];

    wifi_event_stru wifi_event_cb = {0};

    wifi_event_cb.wifi_event_scan_state_changed = wifi_scan_state_changed;
    wifi_event_cb.wifi_event_connection_changed = wifi_connection_changed;
    /* 注册事件回调 */
    if (wifi_register_event_cb(&wifi_event_cb) != 0) {
        printf("wifi_event_cb register fail.\r\n");
        return -1;
    }
    printf("wifi_event_cb register succ.\r\n");

    /* 等待wifi初始化完成 */
    while (wifi_is_wifi_inited() == 0) {
        (void)osDelay(10); /* 10: 延时100ms  */
    }
    wifi_connect();

    printf("create socket start! \r\n");
    if ((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) != ERRCODE_SUCC) {
        printf("create socket failed!\r\n");
        return 0;
    }
    printf("create socket succ!\r\n");

    /* 初始化预连接的服务端地址 */
    send_addr.sin_family = AF_INET;
    send_addr.sin_port = htons(CONFIG_SERVER_PORT);
    send_addr.sin_addr.s_addr = inet_addr(CONFIG_SERVER_IP);
    addr_length = sizeof(send_addr);
    while (1) {
        memset(recv_buf, 0, sizeof(recv_buf));
        /* 发送数据到服务远端 */
        printf("sendto...\r\n");
        sendto(sock_fd, SEND_DATA, strlen(SEND_DATA), 0, (struct sockaddr *)&send_addr, addr_length);
        osDelay(100); /* 100: 延时1s  */

        /* 接收服务端返回的字符串 */
        recvfrom(sock_fd, recv_buf, sizeof(recv_buf), 0, (struct sockaddr *)&send_addr, &addr_length);
        printf("recvfrom:%s\n", recv_buf);
    }
    lwip_close(sock_fd);
    return 0;
}

/**
 * @brief 样例任务入口，创建线程
 * @note 创建sta_sample_task任务并启动
 */
static void sta_sample(void)
{
    osThreadAttr_t attr;
    attr.name = "sta_sample_task";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = WIFI_TASK_STACK_SIZE;
    attr.priority = osPriorityNormal;
    if (osThreadNew((osThreadFunc_t)sta_sample_init, NULL, &attr) == NULL) {
        printf("Create sta_sample_task fail.\r\n");
    }
    printf("Create sta_sample_task succ.\r\n");
}

/* Run the sample. */
app_run(sta_sample);