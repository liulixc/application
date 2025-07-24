#ifndef MQTT_DEMO_H
#define MQTT_DEMO_H

// ��������ö�ٶ���
typedef enum {
    NET_TYPE_4G,
    NET_TYPE_WIFI,
    NET_TYPE_NONE
} net_type_t;

// ���֧�ֵ�BMS�豸��
#define MAX_BMS_DEVICES 32 // ��״�����£��������ؿɹ��������豸��

typedef struct {
    char receive_payload[256];
    float temperature[5];
    float current;
    float cell_voltages[12]; // �����ѹ
    float total_voltage; // �ܵ�ѹ
    uint8_t soc; // ���SOC
} MQTT_msg;

typedef struct {
    float temperature[5]; // �¶�
    float current;     // ����
    float cell_voltages[12]; // �����ѹ
    float total_voltage; // �ܵ�ѹ
    uint8_t soc; // ���SOC
    uint8_t level; // 节点层级
    uint8_t child; // 子节点数量
} environment_msg;
#endif