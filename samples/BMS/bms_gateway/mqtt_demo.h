#ifndef MQTT_DEMO_H
#define MQTT_DEMO_H

// 网络类型枚举定义
typedef enum {
    NET_TYPE_4G,
    NET_TYPE_WIFI,
    NET_TYPE_NONE
} net_type_t;

// 最大支持的BMS设备数
#define MAX_BMS_DEVICES 32 // 树状网络下，增加网关可管理的总设备数

typedef struct {
    char receive_payload[256];
    float temperature[5];
    float current;
    float cell_voltages[12]; // 单体电压
    float total_voltage; // 总电压
} MQTT_msg;

typedef struct {
    float temperature[5]; // 温度
    float current;     // 电流
    float cell_voltages[12]; // 单体电压
    float total_voltage; // 总电压
} environment_msg;
#endif