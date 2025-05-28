/**
 * @file wifi_connect.c
 * @brief Wi-Fi STA模式连接流程实现文件
 *
 * 本文件实现了Wi-Fi STA模式下的扫描、连接、DHCP获取IP等主要流程，
 * 包括事件回调注册、目标AP筛选、连接状态与DHCP状态检测等。
 * 适用于HiSilicon平台的Wi-Fi连接样例。
 *
 * 主要流程：
 * 1. 注册Wi-Fi事件回调，等待Wi-Fi初始化完成。
 * 2. 启动扫描，筛选目标AP。
 * 3. 连接目标AP，获取IP地址。
 * 4. 检查连接与DHCP状态。
 *
 * 主要函数说明：
 * - wifi_scan_state_changed：扫描完成事件回调。
 * - wifi_connection_changed：连接状态变化事件回调。
 * - example_get_match_network：筛选目标AP并填充连接参数。
 * - example_check_connect_status：查询STA连接状态。
 * - example_check_dhcp_status：查询DHCP分配IP状态。
 * - example_sta_function：STA连接主流程。
 * - wifi_connect：对外提供的Wi-Fi连接接口。
 */

#include "lwip/netifapi.h"
#include "wifi_hotspot.h"
#include "wifi_hotspot_config.h"
#include "td_base.h"
#include "td_type.h"
#include "stdlib.h"
#include "uart.h"
#include "cmsis_os2.h"
#include "app_init.h"
#include "soc_osal.h"
#include "lwip/nettool/misc.h"
#include "wifi_connect.h"

#define WIFI_IFNAME_MAX_SIZE 16
#define WIFI_MAX_SSID_LEN 33
#define WIFI_SCAN_AP_LIMIT 64
#define WIFI_MAC_LEN 6
#define WIFI_STA_SAMPLE_LOG "[WIFI_STA_SAMPLE]"
#define WIFI_NOT_AVALLIABLE 0
#define WIFI_AVALIABE 1
#define WIFI_GET_IP_MAX_COUNT 300

#define WIFI_TASK_PRIO (osPriority_t)(13)
#define WIFI_TASK_DURATION_MS 2000
#define WIFI_TASK_STACK_SIZE 0x1000

static td_void wifi_scan_state_changed(td_s32 state, td_s32 size);
static td_void wifi_connection_changed(td_s32 state, const wifi_linked_info_stru *info, td_s32 reason_code);

wifi_event_stru wifi_event_cb = {
    .wifi_event_connection_changed = wifi_connection_changed,
    .wifi_event_scan_state_changed = wifi_scan_state_changed,
};

enum {
    WIFI_STA_SAMPLE_INIT = 0,     /* 0:初始态 */
    WIFI_STA_SAMPLE_SCANING,      /* 1:扫描中 */
    WIFI_STA_SAMPLE_SCAN_DONE,    /* 2:扫描完成 */
    WIFI_STA_SAMPLE_FOUND_TARGET, /* 3:匹配到目标AP */
    WIFI_STA_SAMPLE_CONNECTING,   /* 4:连接中 */
    WIFI_STA_SAMPLE_CONNECT_DONE, /* 5:关联成功 */
    WIFI_STA_SAMPLE_GET_IP,       /* 6:获取IP */
} g_wifi_state_enum;

static td_u8 g_wifi_state = WIFI_STA_SAMPLE_INIT;

/**
 * @brief STA扫描事件回调函数
 * @param state 扫描状态
 * @param size 扫描到的AP数量
 * @note 扫描完成后设置全局状态为SCAN_DONE
 */
static td_void wifi_scan_state_changed(td_s32 state, td_s32 size)
{
    UNUSED(state);
    UNUSED(size);
    PRINT("%s::Scan done!.\r\n", WIFI_STA_SAMPLE_LOG);
    g_wifi_state = WIFI_STA_SAMPLE_SCAN_DONE;
    return;
}

/**
 * @brief STA连接状态事件回调函数
 * @param state 连接状态
 * @param info 连接信息
 * @param reason_code 失败原因码
 * @note 连接成功或失败时设置全局状态
 */
static td_void wifi_connection_changed(td_s32 state, const wifi_linked_info_stru *info, td_s32 reason_code)
{
    UNUSED(info);
    UNUSED(reason_code);

    if (state == WIFI_NOT_AVALLIABLE) {
        PRINT("%s::Connect fail!. try agin !\r\n", WIFI_STA_SAMPLE_LOG);
        g_wifi_state = WIFI_STA_SAMPLE_INIT;
    } else {
        PRINT("%s::Connect succ!.\r\n", WIFI_STA_SAMPLE_LOG);
        g_wifi_state = WIFI_STA_SAMPLE_CONNECT_DONE;
    }
}

/**
 * @brief 筛选扫描到的目标AP并填充连接参数
 * @param expected_bss 期望连接的AP参数结构体
 * @param ssid 目标AP的SSID
 * @param psk 目标AP的密码
 * @return 0: 成功, -1: 失败
 * @note 先扫描所有AP，找到目标后填充expected_bss
 */
td_s32 example_get_match_network(wifi_sta_config_stru *expected_bss, const char *ssid, const char *psk)
{
    td_s32 ret;
    td_u32 num = 64; /* 64:扫描到的Wi-Fi网络数量 */
    // td_char ssid[] = "test";
    // td_char key[] = "123456789"; /* 待连接的网络接入密码 */
    td_bool find_ap = TD_FALSE;
    td_u8 bss_index;
    /* 获取扫描结果 */
    td_u32 scan_len = sizeof(wifi_scan_info_stru) * WIFI_SCAN_AP_LIMIT;
    wifi_scan_info_stru *result = osal_kmalloc(scan_len, OSAL_GFP_ATOMIC);
    if (result == TD_NULL) {
        return -1;
    }
    memset_s(result, scan_len, 0, scan_len);
    ret = wifi_sta_get_scan_info(result, &num);
    if (ret != 0) {
        osal_kfree(result);
        return -1;
    }
    /* 筛选扫描到的Wi-Fi网络，选择待连接的网络 */
    for (bss_index = 0; bss_index < num; bss_index++) {
        if (strlen(ssid) == strlen(result[bss_index].ssid)) {
            if (memcmp(ssid, result[bss_index].ssid, strlen(ssid)) == 0) {
                find_ap = TD_TRUE;
                break;
            }
        }
    }
    /* 未找到待连接AP,可以继续尝试扫描或者退出 */
    if (find_ap == TD_FALSE) {
        osal_kfree(result);
        return -1;
    }
    /* 找到网络后复制网络信息和接入密码 */
    if (memcpy_s(expected_bss->ssid, WIFI_MAX_SSID_LEN, ssid, strlen(ssid)) != 0) {
        osal_kfree(result);
        return -1;
    }
    if (memcpy_s(expected_bss->bssid, WIFI_MAC_LEN, result[bss_index].bssid, WIFI_MAC_LEN) != 0) {
        osal_kfree(result);
        return -1;
    }
    expected_bss->security_type = result[bss_index].security_type;
    if (memcpy_s(expected_bss->pre_shared_key, WIFI_MAX_SSID_LEN, psk, strlen(psk)) != 0) {
        osal_kfree(result);
        return -1;
    }
    expected_bss->ip_type = 1; /* 1：IP类型为动态DHCP获取 */
    osal_kfree(result);
    return 0;
}

/**
 * @brief 查询STA连接状态
 * @return 0: 连接成功, -1: 连接失败
 * @note 最多查询5次，每次间隔500ms
 */
td_bool example_check_connect_status(td_void)
{
    td_u8 index;
    wifi_linked_info_stru wifi_status;
    /* 获取网络连接状态，共查询5次，每次间隔500ms */
    for (index = 0; index < 5; index++) {
        (void)osDelay(50); /* 50: 延时500ms */
        memset_s(&wifi_status, sizeof(wifi_linked_info_stru), 0, sizeof(wifi_linked_info_stru));
        if (wifi_sta_get_ap_info(&wifi_status) != 0) {
            continue;
        }
        if (wifi_status.conn_state == 1) {
            return 0; /* 连接成功退出循环 */
        }
    }
    return -1;
}

/**
 * @brief 查询DHCP分配IP状态
 * @param netif_p 网络接口指针
 * @param wait_count 等待计数器指针
 * @return 0: DHCP成功, -1: 失败或超时
 * @note 超时会重置状态，需重新连接
 */
td_bool example_check_dhcp_status(struct netif *netif_p, td_u32 *wait_count)
{
    if ((ip_addr_isany(&(netif_p->ip_addr)) == 0) && (*wait_count <= WIFI_GET_IP_MAX_COUNT)) {
        /* DHCP成功 */
        PRINT("%s::STA DHCP success.\r\n", WIFI_STA_SAMPLE_LOG);
        return 0;
    }

    if (*wait_count > WIFI_GET_IP_MAX_COUNT) {
        PRINT("%s::STA DHCP timeout, try again !.\r\n", WIFI_STA_SAMPLE_LOG);
        *wait_count = 0;
        g_wifi_state = WIFI_STA_SAMPLE_INIT;
    }
    return -1;
}

/**
 * @brief STA连接主流程
 * @param ssid 目标AP的SSID
 * @param psk 目标AP的密码
 * @return 0: 成功, -1: 失败
 * @note 包含扫描、连接、DHCP获取IP等完整流程
 */
td_s32 example_sta_function(const char *ssid, const char *psk)
{
    td_char ifname[WIFI_IFNAME_MAX_SIZE + 1] = "wlan0"; /* 创建的STA接口名 */
    wifi_sta_config_stru expected_bss = {0};            /* 连接请求信息 */
    struct netif *netif_p = TD_NULL;
    td_u32 wait_count = 0;

    /* 创建STA接口 */
    if (wifi_sta_enable() != 0) {
        return -1;
    }
    PRINT("%s::STA enable succ.\r\n", WIFI_STA_SAMPLE_LOG);

    do {
        (void)osDelay(1); /* 1: 等待10ms后判断状态 */
        if (g_wifi_state == WIFI_STA_SAMPLE_INIT) 
        {
            PRINT("%s::Scan start!\r\n", WIFI_STA_SAMPLE_LOG);
            g_wifi_state = WIFI_STA_SAMPLE_SCANING;
            /* 启动STA扫描 */
            if (wifi_sta_scan() != 0) 
            {
                g_wifi_state = WIFI_STA_SAMPLE_INIT;
                continue;
            }
        } 
        else if (g_wifi_state == WIFI_STA_SAMPLE_SCAN_DONE) 
        {
            /* 获取待连接的网络 */
            if (example_get_match_network(&expected_bss, ssid, psk) != 0) 
            {
                PRINT("%s::Do not find AP, try again !\r\n", WIFI_STA_SAMPLE_LOG);
                g_wifi_state = WIFI_STA_SAMPLE_INIT;
                continue;
            }
            g_wifi_state = WIFI_STA_SAMPLE_FOUND_TARGET;
        } 
        else if (g_wifi_state == WIFI_STA_SAMPLE_FOUND_TARGET) 
        {
            PRINT("%s::Connect start.\r\n", WIFI_STA_SAMPLE_LOG);
            g_wifi_state = WIFI_STA_SAMPLE_CONNECTING;
            /* 启动连接 */
            if (wifi_sta_connect(&expected_bss) != 0) 
            {
                g_wifi_state = WIFI_STA_SAMPLE_INIT;
                continue;
            }
        } 
        else if (g_wifi_state == WIFI_STA_SAMPLE_CONNECT_DONE) 
        {
            PRINT("%s::DHCP start.\r\n", WIFI_STA_SAMPLE_LOG);
            g_wifi_state = WIFI_STA_SAMPLE_GET_IP;
            netif_p = netifapi_netif_find(ifname);
            if (netif_p == TD_NULL || netifapi_dhcp_start(netif_p) != 0) {
                PRINT("%s::find netif or start DHCP fail, try again !\r\n", WIFI_STA_SAMPLE_LOG);
                g_wifi_state = WIFI_STA_SAMPLE_INIT;
                continue;
            }
        } else if (g_wifi_state == WIFI_STA_SAMPLE_GET_IP) 
        {
            if (example_check_dhcp_status(netif_p, &wait_count) == 0) 
            {
                netifapi_netif_common(netif_p, dhcp_clients_info_show, NULL);
                break;
            }
            wait_count++;
        }
    } while (1);

    return 0;
}

/**
 * @brief 对外提供的Wi-Fi连接接口
 * @param ssid 目标AP的SSID
 * @param psk 目标AP的密码
 * @return 0: 成功, -1: 失败
 * @note 注册事件回调，等待初始化，调用主流程
 */
int wifi_connect(const char *ssid, const char *psk)
{
    /* 注册事件回调 */
    if (wifi_register_event_cb(&wifi_event_cb) != 0) {
        PRINT("%s::wifi_event_cb register fail.\r\n", WIFI_STA_SAMPLE_LOG);
        return -1;
    }
    PRINT("%s::wifi_event_cb register succ.\r\n", WIFI_STA_SAMPLE_LOG);

    /* 等待wifi初始化完成 */
    while (wifi_is_wifi_inited() == 0) {
        (void)osDelay(10); /* 1: 等待100ms后判断状态 */
    }
    PRINT("%s::wifi init succ.\r\n", WIFI_STA_SAMPLE_LOG);

    if (example_sta_function(ssid, psk) != 0) {
        PRINT("%s::example_sta_function fail.\r\n", WIFI_STA_SAMPLE_LOG);
        return -1;
    }
    return 0;
}
