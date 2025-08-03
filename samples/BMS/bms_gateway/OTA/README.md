# BMS网关OTA升级模块

## 概述

本模块将原本位于 `/root/fbb_ws63/src/application/samples/BMS/OTA/ota_task.c` 的OTA功能封装到BMS网关项目中，实现了通过MQTT命令触发的远程固件升级功能。

## 文件结构

```
OTA/
├── ota_task.c      # OTA核心实现文件
├── ota_task.h      # OTA头文件，包含函数声明
└── README.md       # 本说明文件
```

## 主要功能

### 1. OTA准备 (`ota_prepare`)
- 初始化分区模块
- 检查升级文件大小限制
- 准备升级环境

### 2. HTTP客户端下载 (`http_clienti_get`)
- 通过WiFi连接到升级服务器
- 下载升级文件
- 实时写入升级分区
- 完成后自动重启系统

### 3. OTA任务启动 (`ota_task_start`)
- 创建独立的OTA任务线程
- 可被其他模块调用启动升级流程

## 集成到MQTT网关

### 命令格式

MQTT命令中包含以下JSON格式可触发OTA升级：

```json
{
  "command_name": "ota_upgrade",
  "paras": {
    "version": "1.0.1"
  }
}
```

### 响应格式

**成功响应：**
```json
{
  "result_code": 0,
  "response_name": "ota_upgrade",
  "paras": {
    "result": "ota_started"
  }
}
```

**失败响应：**
```json
{
  "result_code": 1,
  "response_name": "ota_upgrade",
  "paras": {
    "result": "ota_start_failed"
  }
}
```

## 配置参数

在 `ota_task.c` 中可以修改以下配置：

```c
#define SSID  "QQ"                    // WiFi SSID
#define PASSWORD "tangyuan"           // WiFi密码
#define SERVER_IP "1.13.92.135"       // 升级服务器IP
#define SOCK_TARGET_PORT 8080         // 服务器端口
```

升级文件请求路径：
```c
static const char *g_request = 
    "GET /test.bin HTTP/1.1\r\n"
    "Host: 1.13.92.135:8080\r\n"
    "Connection: close\r\n"
    "\r\n";
```

## 使用方法

### 1. 通过MQTT命令触发（推荐）

发送包含 `"command_name":"ota_upgrade"` 的MQTT命令到设备，系统会自动识别并启动OTA升级流程。

### 2. 程序内部调用

```c
#include "OTA/ota_task.h"

// 启动OTA任务
if (ota_task_start() == 0) {
    printf("OTA任务启动成功\n");
} else {
    printf("OTA任务启动失败\n");
}
```

## 升级流程

1. 接收MQTT OTA命令
2. 启动OTA任务线程
3. 连接WiFi网络
4. 建立HTTP连接到升级服务器
5. 下载升级文件
6. 实时写入升级分区
7. 验证数据完整性
8. 请求系统升级
9. 自动重启完成升级

## 注意事项

1. **网络要求**：升级过程需要稳定的WiFi连接
2. **存储空间**：确保有足够的存储空间存放升级文件
3. **电源稳定**：升级过程中请保持电源稳定，避免断电
4. **服务器配置**：确保升级服务器可访问且升级文件可用
5. **任务优先级**：OTA任务使用普通优先级，不会影响其他关键任务

## 依赖模块

- WiFi连接模块 (`../wifi/wifi_connect.h`)
- 升级模块 (`upg.h`, `upg_porting.h`)
- 分区模块 (`partition.h`)
- LWIP网络栈
- CMSIS-RTOS2

## 错误处理

模块包含完整的错误处理机制：
- 网络连接失败检测
- 文件大小验证
- 数据完整性检查
- 升级失败回滚保护

所有错误信息都会通过串口输出，便于调试和问题定位。