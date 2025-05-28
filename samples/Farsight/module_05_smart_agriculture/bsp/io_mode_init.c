#include "io_mode_init.h"
#include "stdio.h"
#include "hal_bsp_ssd1306.h"
// OLED显示屏显示任务
char oledShowBuff[50] = {0};
uint8_t pwm_flag = 0;
system_value_t systemdata = {0};
pwm_config_t PWM_LOW = {200, // 低电平
                        10,  // 高电平
                        0, 0, true};
pwm_config_t PWM_HIGH = {10,  // 低电平
                         200, // 高电平持续tick 时间 = tick * (1/32000000)
                         0, 0, true};
void user_key_callback_func(pin_t pin, uintptr_t param)
{
    unused(pin);
    unused(param);
    pwm_flag++;
    printf("pwm_flag:%d", pwm_flag);
}
void user_key_init(void)
{
    uapi_pin_set_mode(USER_KEY, PIN_MODE_0);

    uapi_pin_set_mode(USER_KEY, 0);
    uapi_pin_set_pull(USER_KEY, PIN_PULL_TYPE_DOWN);
    uapi_gpio_set_dir(USER_KEY, GPIO_DIRECTION_INPUT);
    /* 注册指定GPIO下降沿中断，回调函数为gpio_callback_func */
    if (uapi_gpio_register_isr_func(USER_KEY, GPIO_INTERRUPT_RISING_EDGE, user_key_callback_func) != 0) {
        printf("register_isr_func fail!\r\n");
        uapi_gpio_unregister_isr_func(USER_KEY);
    }
    uapi_gpio_enable_interrupt(USER_KEY);
}
void user_led_init(void)
{
    uapi_pin_set_mode(USER_LED, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(USER_LED, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(USER_LED, GPIO_LEVEL_HIGH);
}

void user_pwm_init(void)
{

    uapi_pin_set_mode(USER_PWM1, PIN_MODE_1);
    uapi_pwm_init();
    uapi_pwm_open(PWM1_CHANNEL, &PWM_LOW);

    uint8_t channel_id = PWM1_CHANNEL;
    uapi_pwm_set_group(1, &channel_id, 1);
    uapi_pwm_start(PWM1_CHANNEL);

    uapi_pin_set_mode(USER_PWM0, PIN_MODE_1);
    uapi_pwm_init();
    uapi_pwm_open(PWM0_CHANNEL, &PWM_LOW);

    channel_id = PWM0_CHANNEL;
    uapi_pwm_set_group(0, &channel_id, 1);
    uapi_pwm_start(PWM0_CHANNEL);
}

void sensor_io_init(void)
{
    /* 火焰传感器 */
    uapi_pin_set_mode(USER_FLAME, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(USER_FLAME, GPIO_DIRECTION_INPUT);
    /* 可燃气体传感器 */
    uapi_pin_set_mode(USER_GAS, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(USER_GAS, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(USER_GAS, GPIO_LEVEL_HIGH);

    uapi_adc_init(ADC_CLOCK_NONE);
}

void oled_show(void)
{
    if (systemdata.flame_status == SENSOR_OFF) {
        sprintf_s(oledShowBuff, sizeof(oledShowBuff), "FLAME:      OFF");
        SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_0, oledShowBuff, TEXT_SIZE_16);
        memset_s(oledShowBuff, sizeof(oledShowBuff), 0, sizeof(oledShowBuff));
    } else {
        sprintf_s(oledShowBuff, sizeof(oledShowBuff), "FLAME:       ON");
        SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_0, oledShowBuff, TEXT_SIZE_16);
        memset_s(oledShowBuff, sizeof(oledShowBuff), 0, sizeof(oledShowBuff));
    }
    if (sprintf_s(oledShowBuff, sizeof(oledShowBuff), "CO2:      %5d", systemdata.CO2) > 0) {
        SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_1, oledShowBuff, TEXT_SIZE_16);
        memset_s(oledShowBuff, sizeof(oledShowBuff), 0, sizeof(oledShowBuff));
    }
    if (sprintf_s(oledShowBuff, sizeof(oledShowBuff), "TVOC:     %5d", systemdata.TVOC) > 0) {
        SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_2, oledShowBuff, TEXT_SIZE_16);
        memset_s(oledShowBuff, sizeof(oledShowBuff), 0, sizeof(oledShowBuff));
    }
#if (DEIVER_BOARD)
    if (systemdata.gas_status == SENSOR_OFF) {
        sprintf_s(oledShowBuff, sizeof(oledShowBuff), "GAS:        OFF");
        SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_3, oledShowBuff, TEXT_SIZE_16);
        memset_s(oledShowBuff, sizeof(oledShowBuff), 0, sizeof(oledShowBuff));
    } else {
        sprintf_s(oledShowBuff, sizeof(oledShowBuff), "GAS:         ON");
        SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_3, oledShowBuff, TEXT_SIZE_16);
        memset_s(oledShowBuff, sizeof(oledShowBuff), 0, sizeof(oledShowBuff));
    }

#else
    if (sprintf_s(oledShowBuff, sizeof(oledShowBuff), "GAS:      %5d ", systemdata.GAS_MIC) > 0) {
        SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_3, oledShowBuff, TEXT_SIZE_16);
        memset_s(oledShowBuff, sizeof(oledShowBuff), 0, sizeof(oledShowBuff));
    }
#endif
}
