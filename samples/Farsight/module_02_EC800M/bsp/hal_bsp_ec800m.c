#include "hal_bsp_ec800m.h"
#include "bsp_init.h"
#include "hal_bsp_pcf8574.h"

char ec800m_sendbuff[255] = {0};

int at_num = 0;
tsATCmds ATCmds[] = {
    {"ATE0\r\n", "OK"},
    {"AT+QGPSCFG=\"outport\",\"uartdebug\"\r\n", "OK"},
    {"AT+QGPSCFG=\"nmeasrc\",1\r\n", "OK"},
    {"AT+QGPSCFG=\"gpsnmeatype\",63\r\n", "OK"},
    {"AT+QGPSCFG=\"gnssconfig\",0\r\n", "OK"},
    {"AT+QGPSCFG=\"autogps\",0\r\n", "OK"},
    {"AT+QGPSCFG=\"apflash\",0\r\n", "OK"},
    {"AT+QGPS=1\r\n", "OK"},
    {"AT+CSQ\r\n", "OK"},
    {"AT+CPIN?\r\n", "OK"},
    {"AT+CREG?\r\n", "OK"},
    {"AT+CGATT?\r\n", "OK"},
    {"AT+QMTOPEN=0,\"%s\",1883\r\n", "OK"},
    {"AT+QMTCONN=0,\"%s\",\"%s\",\"%s\"\r\n", "OK"},
    {"AT+QMTPUB=0,0,0,0,\"$oc/devices/%s/sys/properties/report\"\r\n", ">"}, // 14
    {"{\"services\":[{\"service_id\":\"attribute\",\"properties\":{\"temperature\":%d,\"humidity\":%d,"
     "\"voltage\":%.1f,\"car\":\"%s\",\"Latitude\":\"%s\",\"Longitude\":\"%s\"}}]}",
     "OK"},
    {"\x1A\0", "OK"},
    {"AT+QGPSLOC=0\r\n", "OK"},
    {"AT+QMTDISC=0\r\n", "OK"},
};
void ec800_pwr_init(void)
{
#if (DEIVER_BOARD)
    tmp_io.bit.p6 = 1;
    PCF8574_Write(tmp_io.all);
    osal_mdelay(3000);
    tmp_io.bit.p6 = 0;
    PCF8574_Write(tmp_io.all);
#else
    uapi_pin_set_mode(EC_PWR, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(EC_PWR, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(EC_PWR, GPIO_LEVEL_LOW);

    uapi_gpio_set_val(EC_PWR, GPIO_LEVEL_HIGH);
    osal_mdelay(3000);
    uapi_gpio_set_val(EC_PWR, GPIO_LEVEL_LOW);
#endif
}
// ec800连接云平台初始化
void ec800_send_init(void)
{
    while (at_num < 14) {

        if (at_num == 12) {
            memset(ec800m_sendbuff, 0, sizeof(ec800m_sendbuff));
            if (sprintf(ec800m_sendbuff, ATCmds[12].ATSendStr, HUWWEIYUN_MQTT_IP) > 0)
                uart_send_buff((uint8_t *)ec800m_sendbuff, strlen(ec800m_sendbuff));
        } else if (at_num == 13) {
            memset(ec800m_sendbuff, 0, sizeof(ec800m_sendbuff));
            if (sprintf(ec800m_sendbuff, ATCmds[13].ATSendStr, MQTT_CLIENT_ID, MQTT_USER_NAME, MQTT_PASS_WORD) > 0)
                uart_send_buff((uint8_t *)ec800m_sendbuff, strlen(ec800m_sendbuff));
        } else {
            memset(ec800m_sendbuff, 0, sizeof(ec800m_sendbuff));
            if (sprintf(ec800m_sendbuff, ATCmds[at_num].ATSendStr) > 0)
                uart_send_buff((uint8_t *)ATCmds[at_num].ATSendStr, strlen(ATCmds[at_num].ATSendStr));
        }

        memset(uart2_recv.recv, 0, sizeof(uart2_recv.recv));
        if (uapi_uart_read(UART_ID, uart2_recv.recv, UART_RECV_SIZE, 0)) {
            printf("[AT RECV %s]\n", uart2_recv.recv);
            if (strstr((char *)uart2_recv.recv, ATCmds[at_num].ATRecStr) != NULL) {
                printf(" 【%s succeed !!!】\n", ATCmds[at_num].ATSendStr);
                at_num++;
            } else {
                printf(" 【%s fail !!!】\n", ATCmds[at_num].ATSendStr);
            }
            memset(uart2_recv.recv, 0, sizeof(uart2_recv.recv));
        }
    }
}
void get_gps_coordinates(const char *input)
{
    const char *locLine = strstr(input, "+QGPSLOC:");
    if (locLine == NULL) {
        return;
    }
    locLine += strlen("+QGPSLOC:"); // 跳过 "+QGPSLOC:"

    // 跳过第一个逗号分隔的部分（时间信息）
    while (*locLine != ',' && *locLine != '\0') {
        locLine++;
    }
    locLine++; // 跳过逗号

    // 提取纬度信息
    uint8_t i = 0;
    while ((*locLine != ',') && (i < sizeof(latitude) - 2)) {
        latitude[i++] = *locLine++;
    }
    latitude[i] = '\0';
    locLine++; // 跳过逗号

    // 提取经度信息
    i = 0;
    while ((*locLine != ',') && (i < sizeof(longitude) - 2)) {
        longitude[i++] = *locLine++;
    }
    longitude[i] = '\0';
}
void get_car_cmd(const char *input)
{
    const char *iot_cmd = strstr(input, "+QMTRECV:");
    if (iot_cmd == NULL) {
        return;
    }
    if (strstr(input, "CAR_STATUS_RUN") != NULL) {
        systemValue.car_status = CAR_STATUS_RUN;

    } else if (strstr(input, "CAR_STATUS_BACK") != NULL) {
        systemValue.car_status = CAR_STATUS_BACK;

    } else if (strstr(input, "CAR_STATUS_LEFT") != NULL) {
        systemValue.car_status = CAR_STATUS_LEFT;

    } else if (strstr(input, "CAR_STATUS_RIGHT") != NULL) {
        systemValue.car_status = CAR_STATUS_RIGHT;
    }
}