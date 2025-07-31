# WS63 OTA 升级学习文档

## 概述

本文档基于 `ota_task.c` 文件，详细介绍了 WS63 芯片的 OTA（Over-The-Air）固件升级实现。该实现通过 HTTP 协议从远程服务器下载固件包，并使用系统提供的升级 API 完成固件更新。

## 文件结构

### 头文件依赖
```c
#include "string.h"
#include "lwip/netifapi.h"     // LWIP 网络接口
#include "wifi_hotspot.h"       // WiFi 热点功能
#include "lwip/sockets.h"       // Socket 编程接口
#include "upg.h"                // 升级模块接口
#include "partition.h"          // 分区管理
```

### 关键宏定义
```c
#define WIFI_TASK_STACK_SIZE 0x2000    // WiFi 任务栈大小
#define RECV_BUFFER_SIZE     1024      // 接收缓冲区大小
#define SOCK_TARGET_PORT     8080      // 目标服务器端口
#define SSID                 "QQ"      // WiFi SSID
#define PASSWORD             "tangyuan" // WiFi 密码
#define SERVER_IP            "1.13.92.135" // 服务器 IP 地址
```

## 核心功能模块

### 1. OTA 准备模块 (`ota_prepare`)

**功能**：初始化升级环境，检查存储空间，准备升级参数。

**实现步骤**：
1. 初始化分区模块 (`uapi_partition_init`)
2. 获取可用存储空间 (`uapi_upg_get_storage_size`)
3. 验证固件大小是否超出限制
4. 配置升级参数并调用 `uapi_upg_prepare`

```c
errcode_t ota_prepare(uint32_t file_size)
{
    // 1. 分区初始化
    ret = uapi_partition_init();
    
    // 2. 检查存储空间
    max_len = uapi_upg_get_storage_size();
    if(file_size > max_len) {
        return -1;
    }
    
    // 3. 准备升级
    upg_prepare_info_t upg_prepare_info;
    upg_prepare_info.package_len = file_size;
    ret = uapi_upg_prepare(&upg_prepare_info);
    
    return ERRCODE_SUCC;
}
```

### 2. HTTP 客户端模块 (`http_clienti_get`)

**功能**：通过 HTTP 协议下载固件包并执行 OTA 升级。

#### 2.1 网络连接建立
```c
// WiFi 连接
wifi_connect(SSID, PASSWORD);

// Socket 创建和连接
int sockfd = socket(AF_INET, SOCK_STREAM, 0);
struct sockaddr_in addr = {0};
addr.sin_family = AF_INET;
addr.sin_port = htons(SOCK_TARGET_PORT);
addr.sin_addr.s_addr = inet_addr(SERVER_IP);
connect(sockfd, (struct sockaddr *)&addr, sizeof(addr));
```

#### 2.2 HTTP 请求发送
```c
static const char *g_request = 
    "GET /test.fwpkg HTTP/1.1\r\n"
    "Host: 1.13.92.135:8080\r\n"
    "Connection: close\r\n"
    "\r\n";
    
send(sockfd, g_request, strlen(g_request), 0);
```

#### 2.3 HTTP 响应头解析

**关键特性**：
- 使用 2048 字节缓冲区累积响应头
- 通过 `\r\n\r\n` 标识响应头结束
- 使用 `strstr` 解析 `Content-Length`
- 处理第一个数据包中可能包含的响应体数据

```c
static char header_buffer[2048] = {0};
static int header_offset = 0;

// 累积响应头数据
memcpy(header_buffer + header_offset, recv_buffer, bytes_received);
header_offset += bytes_received;

// 查找响应头结束标记
char *head_end = strstr(header_buffer, "\r\n\r\n");
if (head_end) {
    int header_length = (head_end - header_buffer) + 4;
    body_data_in_first_packet = header_offset - header_length;
    
    // 解析 Content-Length
    char *content_length_str = strstr(header_buffer, "Content-Length:");
    if (content_length_str) {
        content_length_str += strlen("Content-Length:");
        file_size = atoi(content_length_str);
    }
}
```

#### 2.4 固件数据接收与写入

**数据处理流程**：
1. 处理第一个包中的响应体数据
2. 循环接收剩余数据
3. 实时写入到 Flash 存储
4. 进度监控和完整性检查

```c
// 处理第一个包中的响应体数据
if (body_data_in_first_packet > 0) {
    uapi_upg_write_package_sync(0, header_buffer + header_length, write_size);
    total_recieved = write_size;
}

// 循环接收剩余数据
while (total_recieved < file_size) {
    int bytes_received = recv(sockfd, recv_buffer, sizeof(recv_buffer), 0);
    
    // 防止数据超量
    int write_size = bytes_received;
    if (total_recieved + bytes_received > file_size) {
        write_size = file_size - total_recieved;
    }
    
    // 写入数据
    uapi_upg_write_package_sync(total_recieved, recv_buffer, write_size);
    total_recieved += write_size;
    
    // 进度监控
    if (total_recieved % 1024 == 0 || total_recieved == file_size) {
        int progress_percent = (total_recieved * 100) / file_size;
        osal_printk("[ota task] : progress checkpoint: %d/%d (%d%%)\r\n", 
                   total_recieved, file_size, progress_percent);
    }
}
```

#### 2.5 升级执行

**最终步骤**：
1. 数据完整性检查
2. 请求系统升级
3. 系统重启

```c
// 数据完整性检查
if (total_recieved != file_size) {
    osal_printk("[ota task] : data incomplete!\r\n");
    return -1;
}

// 请求升级
ret = uapi_upg_request_upgrade(false);
if (ret != ERRCODE_SUCC) {
    osal_printk("[ota task] : upgrade error = 0x%x\r\n", ret);
    return -1;
}

// 系统重启
upg_reboot();
```

## 关键 API 接口

### 升级模块 API

| API 函数 | 功能描述 |
|----------|----------|
| `uapi_partition_init()` | 初始化分区模块 |
| `uapi_upg_get_storage_size()` | 获取升级存储空间大小 |
| `uapi_upg_prepare()` | 准备升级环境 |
| `uapi_upg_write_package_sync()` | 同步写入固件数据 |
| `uapi_upg_request_upgrade()` | 请求执行升级 |
| `upg_reboot()` | 系统重启 |
| `upg_watchdog_kick()` | 喂狗操作 |

### 网络模块 API

| API 函数 | 功能描述 |
|----------|----------|
| `wifi_connect()` | WiFi 连接 |
| `socket()` | 创建 Socket |
| `connect()` | 建立 TCP 连接 |
| `send()` | 发送数据 |
| `recv()` | 接收数据 |
| `lwip_close()` | 关闭连接 |

## 错误处理机制

### 1. 网络错误处理
- Socket 创建失败
- 连接建立失败
- 数据发送/接收失败
- 连接意外断开

### 2. 数据完整性检查
- Content-Length 解析失败
- 接收数据不完整
- 数据超量保护

### 3. 升级过程错误
- 存储空间不足
- 升级准备失败
- 固件写入失败
- 升级请求失败

## 调试和监控

### 调试打印
```c
// 关键节点打印
osal_printk("[ota task] : sock connect succ\r\n");
osal_printk("[ota task] : parsed Content-Length: %d\r\n", file_size);
osal_printk("[ota task] : progress checkpoint: %d/%d (%d%%)\r\n", ...);
osal_printk("[ota task] : recv all succ\r\n");
```

### 进度监控
- 每 1024 字节打印一次进度
- 显示百分比进度
- 数据完整性实时检查

## 注意事项

### 1. 内存管理
- 使用静态缓冲区避免动态分配
- 及时清空接收缓冲区
- 控制响应头缓冲区大小

### 2. 网络稳定性
- 实现连接断开检测
- 添加超时处理机制
- 考虑网络重连策略

### 3. 固件安全
- 验证固件包完整性
- 检查固件包格式
- 实现回滚机制

### 4. 系统兼容性
- 避免使用浮点数格式化（嵌入式系统限制）
- 使用整数计算替代浮点运算
- 注意字符串格式化的兼容性

## 优化建议

### 1. 网络优化
- 实现 HTTP Range 请求支持断点续传
- 添加重试机制
- 优化接收缓冲区大小

### 2. 性能优化
- 减少不必要的调试打印
- 优化数据拷贝次数
- 实现异步写入

### 3. 可靠性提升
- 添加 MD5/SHA256 校验
- 实现升级失败回滚
- 增强错误恢复能力

## 总结

本 OTA 实现提供了一个完整的固件升级解决方案，包括网络通信、数据处理、固件写入和系统升级等核心功能。代码结构清晰，错误处理完善，适合作为 WS63 平台 OTA 升级的参考实现。

通过学习本代码，可以深入理解：
- HTTP 协议在嵌入式系统中的应用
- 固件升级的完整流程
- 嵌入式系统的网络编程
- 系统级升级 API 的使用
- 错误处理和调试技巧