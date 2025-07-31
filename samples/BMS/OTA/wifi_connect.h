#ifndef WIFI_CONNECT_H
#define WIFI_CONNECT_H
#include "errcode.h"

#define CONFIG_WIFI_SSID "QQ"      // 要连接的WiFi 热点账号
#define CONFIG_WIFI_PWD "tangyuan"        // 要连接的WiFi 热点密码
#define CONFIG_SERVER_IP "1.13.92.135" // 要连接的服务器IP
#define CONFIG_SERVER_PORT 8080            // 要连接的服务器端口
errcode_t wifi_connect(void);
#endif