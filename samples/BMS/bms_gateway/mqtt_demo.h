#ifndef MQTT_DEMO_H
#define MQTT_DEMO_H

// 网络类型枚举定义
typedef enum {
    NET_TYPE_4G,
    NET_TYPE_WIFI,
    NET_TYPE_NONE
} net_type_t;

// 最大支持的BMS设备数
#define MAX_BMS_DEVICES 32 // 根据实际情况，网关最多可管理的设备数

typedef struct {
    char receive_payload[256];
} MQTT_msg;

typedef struct {
    float temperature[5]; // 温度
    float current;     // 电流
    float cell_voltages[12]; // 电池电压
    float total_voltage; // 总电压
    uint8_t soc; // SOC
    uint8_t level; // 节点层级
    char child[32]; // 子节点MAC地址后两位
} environment_msg;
#endif