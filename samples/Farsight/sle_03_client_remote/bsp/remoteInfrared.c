#include "remoteInfrared.h"

/************************************************************************
-------------------------红外接收协议--------------------------
开始拉低9ms,接着是一个4.5ms的高脉冲,通知器件开始传送数据了
接着是发送4个8位二进制码,第一二个是遥控识别码(REMOTE_ID),第一个为
正码(0),第二个为反码(255),接着两个数据是键值,第一个为正码
第二个为反码.发送完后40ms,遥控再发送一个9ms低,2ms高的脉冲,
表示按键的次数,出现一次则证明只按下了一次,如果出现多次,则可
以认为是持续按下该键.

*************************************************************************/
void user_ir_start(void)
{
    pwm_config_t PWM_level = {1052, 1052, 0, 0, true}; // 1/38ms=26us
    uapi_pin_set_mode(USER_PWM5, PIN_MODE_1);
    uapi_pwm_init();

    uapi_pwm_open(PWM_CHANNEL5, &PWM_level);
    uint8_t channel_id = PWM_CHANNEL5;
    uapi_pwm_set_group(1, &channel_id, 1);
}

// mode-0 空闲 mode-载波
void ir_Encode_38KHz(uint8_t mode, uint16_t time)
{

    if (mode) // 载波
    {
        uapi_pwm_start_group(1);
        osal_udelay(time);
        uapi_pwm_stop_group(1);
    } else // 空闲
    {
        uapi_pwm_stop_group(1);
        osal_udelay(time + 40);
    }
}
void ir_Encode(uint8_t user, uint8_t key)
{
    uint8_t i, j;
    uint8_t data[4] = {user, ~user, key, ~key};

    ir_Encode_38KHz(1, 9000);
    ir_Encode_38KHz(0, 4500);
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 8; j++) {
            ir_Encode_38KHz(1, 560);
            if ((data[i] & 0x80 >> j))
                ir_Encode_38KHz(0, 1690);
            else
                ir_Encode_38KHz(0, 560);
        }
    }
    ir_Encode_38KHz(1, 680);
}
void ir_send_str(char *buf)
{
    if (strstr(buf, "CAR_GO") != NULL) {
        ir_Encode(0x00, 0xC2);
    } else if (strstr(buf, "CAR_BACK") != NULL) {
        ir_Encode(0x00, 0x50);
    } else if (strstr(buf, "CAR_LEFT") != NULL) {
        ir_Encode(0x00, 0x60);
    } else if (strstr(buf, "CAR_RIGHT") != NULL) {
        ir_Encode(0x00, 0x70);
    } else if (strstr(buf, "CAR_OK") != NULL) {
        ir_Encode(0x00, 0x40);
    } else if (strstr(buf, "CAR_SUBTRACT") != NULL) {
        ir_Encode(0x00, 0x1A);
    } else if (strstr(buf, "CAR_ADD") != NULL) {
        ir_Encode(0x00, 0xD8);
    } else if (strstr(buf, "CAR_CONFIG") != NULL) {
        ir_Encode(0x00, 0xF2);
    } else if (strstr(buf, "CAR_HOME") != NULL) {
        ir_Encode(0x00, 0x72);
    } else if (strstr(buf, "CAR_OFF") != NULL) {
        ir_Encode(0x00, 0xEA);
    } else if (strstr(buf, "CAR_MENU") != NULL) {
        ir_Encode(0x00, 0x58);
    } else if (strstr(buf, "CAR_RETURN") != NULL) {
        ir_Encode(0x00, 0xB2);
    } else {
        ir_Encode(0x00, 0x40);
    }
}