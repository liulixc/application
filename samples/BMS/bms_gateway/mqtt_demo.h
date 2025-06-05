#ifndef MQTT_DEMO_H
#define MQTT_DEMO_H

// 网络类型枚举定义
typedef enum {
    NET_TYPE_4G,
    NET_TYPE_WIFI,
    NET_TYPE_NONE
} net_type_t;

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