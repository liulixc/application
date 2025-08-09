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

### 1. 动态配置管理
- `ota_set_config`: 设置服务器IP、端口和固件路径
- `ota_task_start_with_config`: 使用指定配置启动OTA任务
- 支持运行时修改OTA服务器配置
- 参数验证和错误处理
- 状态重置机制：自动清理失败升级的状态残留，确保后续升级的可靠性
- 直观进度显示：实时显示OTA下载进度百分比，提供清晰的升级状态反馈

### 2. OTA准备 (`ota_prepare`)
- 初始化分区模块
- 检查升级文件大小限制
- 准备升级环境

### 3. HTTP客户端下载 (`http_clienti_get`)
- 动态构建HTTP请求
- 通过WiFi连接到升级服务器
- 下载升级文件
- 实时写入升级分区
- 完成后自动重启系统

### 4. OTA任务启动
- `ota_task_start`: 使用当前配置启动OTA任务
- `ota_task_start_with_config`: 使用指定配置启动OTA任务
- 创建独立的OTA任务线程
- 可被其他模块调用启动升级流程

## 集成到MQTT网关

### MQTT命令格式

#### 1. 使用默认配置的OTA升级

```json
{
  "command_name": "ota_upgrade",
  "paras": {}
}
```

#### 2. 指定设备ID的OTA升级

```json
{
  "command_name": "ota_upgrade",
  "paras": {
    "device_id": "1"
  }
}
```

#### 3. 使用完整动态配置的OTA升级

```json
{
  "command_name": "ota_upgrade",
  "paras": {
    "firmware_path": "/api/firmware/download/test.bin",
    "device_id": "2"
  }
}
```

**参数说明：**
- `firmware_path`: 固件文件路径（字符串）
- `device_id`: 目标设备ID（字符串）

**支持的设备ID：**
- `"1"`: 网关设备
- `"2"` 到 `"12"`: 子设备（BMS设备）
- `"all"` 或 `"*"`: 广播升级（所有设备）
- 其他自定义设备ID

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

**设备ID不匹配响应：**
```json
{
  "result_code": 0,
  "response_name": "ota_upgrade",
  "paras": {
    "result": "device_id_mismatch"
  }
}
```

**失败响应：**
```json
{
  "result_code": 1,
  "response_name": "ota_upgrade",
  "paras": {
    "result": "ota_start_failed"  // 或 "invalid_params", "json_parse_failed"
  }
}
```

## 配置参数

在 `ota_task.c` 中可以修改以下配置：

```c
#define SSID  "QQ"                    // WiFi SSID
#define PASSWORD "tangyuan"           // WiFi密码
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

### 使用方法

### 1. 使用默认配置

发送包含 `"command_name":"ota_upgrade"` 的MQTT命令到设备，系统会使用默认配置启动OTA升级流程。

### 2. 指定目标设备

发送包含设备ID的MQTT命令，可以指定要升级的设备：

```json
{
  "command_name": "ota_upgrade",
  "paras": {
    "device_id": "1"
  }
}
```

### 3. 使用动态配置

发送包含固件路径和目标设备的MQTT命令：

```json
{
  "command_name": "ota_upgrade",
  "paras": {
    "firmware_path": "/firmware/v2.0/device.bin",
    "device_id": "2"
  }
}
```

### 代码示例

```c
#include "OTA/ota_task.h"

// 方式1：使用默认配置启动OTA任务
if (ota_task_start() == 0) {
    printf("OTA任务启动成功\n");
} else {
    printf("OTA任务启动失败\n");
}

// 方式2：使用动态配置启动OTA任务
if (ota_task_start_with_config("/firmware/v2.0/device.bin", "1") == 0) {
    printf("动态配置OTA任务启动成功\n");
} else if (ota_task_start_with_config("/firmware/v2.0/device.bin", "1") == -2) {
    printf("设备ID不匹配，此设备不需要升级\n");
} else {
    printf("动态配置OTA任务启动失败\n");
}

// 方式3：单独设置配置后启动
if (ota_set_config("/firmware/v2.0/device.bin", "1") == 0) {
    if (ota_task_start() == 0) {
        printf("配置设置成功，OTA任务启动成功\n");
    }
}

// 方式4：检查设备ID是否匹配
if (ota_check_device_id("1")) {
    printf("设备ID匹配，可以进行OTA升级\n");
} else {
    printf("设备ID不匹配，跳过OTA升级\n");
}

// 方式5：检查子设备ID
if (ota_check_device_id("5")) {
    printf("子设备ID 5匹配\n");
} else {
    printf("子设备ID 5不匹配当前设备\n");
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

## 问题修复

### 状态残留问题修复

**问题描述**：当请求错误的升级包导致OTA失败后，再次请求正确的升级包仍然会失败。

**原因分析**：HTTP响应头解析使用了静态变量来累积数据，失败后这些静态变量保持了上次的状态，影响后续请求。

**解决方案**：
1. 在每次HTTP请求开始时重置静态变量状态
2. 确保每次OTA任务开始时都从干净的状态开始

**修复效果**：确保每次OTA升级都是从干净的状态开始，避免历史失败对后续升级的影响。

### 参数解析问题修复

**问题描述**：MQTT命令中提供的参数被忽略，系统使用默认值。

**原因分析**：原有的参数解析逻辑要求所有参数都必须提供才会使用动态配置，否则直接使用硬编码的默认值，导致部分提供的参数被忽略。

**解决方案**：重构参数解析逻辑，对每个参数单独判断：
- 如果参数存在且有效，使用提供的值
- 如果参数不存在或无效，使用默认值
- 统一调用 `ota_task_start_with_config` 函数

**修复效果**：现在可以灵活地提供部分参数，系统会正确使用提供的参数值，未提供的参数使用默认值。

### 进度显示优化

**问题描述：** 原有的进度显示不够直观，只在特定条件下显示详细信息，用户难以了解当前升级进度。

**优化方案：** 
- 简化进度显示逻辑，每当进度百分比发生变化时立即显示
- 使用简洁的 `[OTA进度] X%` 格式，清晰易读
- 避免重复显示相同的百分比，减少日志冗余

**优化效果：** 用户可以实时看到清晰的百分比进度，从0%到100%连续显示，提供直观的升级状态反馈。

## 注意事项

1. **网络要求**：升级过程需要稳定的WiFi连接
2. **存储空间**：确保有足够的存储空间存放升级文件
3. **电源稳定**：升级过程中请保持电源稳定，避免断电
4. **服务器配置**：确保升级服务器可访问且升级文件可用
5. **任务优先级**：OTA任务使用普通优先级，不会影响其他关键任务
6. **状态重置**：系统会自动处理状态重置，确保每次升级从干净状态开始

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