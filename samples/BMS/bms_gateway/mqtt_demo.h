#ifndef MQTT_DEMO_H
#define MQTT_DEMO_H

// ��������ö�ٶ���
typedef enum {
    NET_TYPE_4G,
    NET_TYPE_WIFI,
    NET_TYPE_NONE
} net_type_t;

// ���֧�ֵ�BMS�豸��
#define MAX_BMS_DEVICES 32 // ��״�����£��������ؿɹ�������豸��

typedef struct {
    char receive_payload[256];
    float temperature[5];
    float current;
    float cell_voltages[12]; // �����ѹ
    float total_voltage; // �ܵ�ѹ
} MQTT_msg;

typedef struct {
    float temperature[5]; // �¶�
    float current;     // ����
    float cell_voltages[12]; // �����ѹ
    float total_voltage; // �ܵ�ѹ
} environment_msg;
#endif