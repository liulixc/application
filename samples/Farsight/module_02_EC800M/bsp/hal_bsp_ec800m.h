#ifndef EC800_H
#define EC800_H
#include "osal_debug.h"
#include "osal_task.h"
#include "securec.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define EC_PWR GPIO_11
#define HUWWEIYUN_MQTT_IP "e2f41eb4ed.st1.iotda-device.cn-north-4.myhuaweicloud.com"
// MQTT客户端ID
#define MQTT_CLIENT_ID "EC_CAR_800M_0_0_2025022607"
// MQTT用户名
#define MQTT_USER_NAME "EC_CAR_800M"
// MQTT密码
#define MQTT_PASS_WORD "d54efc90552f4ffd3ccdbd1168f812a3c53a5ab62c027c32fba1fd2a59bb095c"

// AT指令结构体
typedef struct {
    char *ATSendStr; // 发送指令
    char *ATRecStr;  // 回复指令
} tsATCmds;
extern tsATCmds ATCmds[];
// extern uint8_t UART2_RECV[512];
void ec800_pwr_init(void);
void ec800_control_data(char *pstr);
void ec800_send_init(void);
void get_gps_coordinates(const char *input);
void get_car_cmd(const char *input);
#endif