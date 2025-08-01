# 简单UART OTA升级功能

本目录实现了最简单的基于UART串口的OTA升级功能。

## 文件说明

- `uart_ota_task.c` - UART OTA主程序
- `firmware.fwpkg` - 示例固件包
- `CMakeLists.txt` - 编译配置文件

## 协议格式

非常简单的协议：
```
[文件大小4字节][固件数据]
```

- 文件大小：4字节，小端序
- 固件数据：直接发送固件文件内容

## 硬件连接

串口连接（参考l610配置）：

```
设备端    ←→    PC
TXD(Pin8) ←→    RXD
RXD(Pin7) ←→    TXD
GND       ←→    GND
```

串口参数：
- 波特率：921600
- 数据位：8
- 停止位：1
- 奇偶校验：无

## 使用方法

1. 打开串口助手，配置串口参数
2. 先发送4字节文件大小（小端序）
3. 再发送固件文件内容

## 升级流程

1. 设备启动，等待接收数据
2. PC发送4字节文件大小
3. 设备准备OTA环境
4. PC发送固件数据
5. 设备接收完成后自动升级重启

## 注意事项

1. 确保串口未被其他程序占用
2. 使用正确的.fwpkg格式固件
3. 传输过程中不要断开连接
4. 确保设备有足够的存储空间

## 调试信息

设备会输出升级进度：
```
[UART OTA]: UART OTA task started
[UART OTA]: Ready for OTA upgrade
[UART OTA]: File size: 12345 bytes
[UART OTA]: OTA started, send firmware data now
[UART OTA]: Progress: 50% (6172/12345)
[UART OTA]: All data received, starting upgrade...
[UART OTA]: Upgrade successful, rebooting...
```