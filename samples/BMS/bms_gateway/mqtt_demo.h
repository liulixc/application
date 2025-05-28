#ifndef MQTT_DEMO_H
#define MQTT_DEMO_H

typedef enum {
    EN_MSG_PARS = 1,
    EN_MSG_REPORT = 2,
} en_msg_type_t;

typedef struct {
    int msg_type;
    char receive_payload[256];
    char temperature[64];
    char current[16];
} MQTT_msg;
#endif