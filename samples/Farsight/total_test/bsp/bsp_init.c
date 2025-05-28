#include "bsp_init.h"
#include "stdio.h"
float temperature = 0;
float humidity = 0;
extern uint16_t ir, als, ps;
// OLED显示屏显示任务
char oledShowBuff[50] = {0};
uint8_t nfc_test;
void user_led_init(void)
{
    uapi_pin_set_mode(USER_LED, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(USER_LED, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(USER_LED, GPIO_LEVEL_HIGH);
}
void user_beep_init(void)
{
    uapi_pin_set_mode(USER_BEEP, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(USER_BEEP, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(USER_BEEP, GPIO_LEVEL_LOW);
}
void user_key_callback_func(pin_t pin, uintptr_t param)
{
    unused(pin);
    unused(param);
    uapi_gpio_toggle(USER_BEEP);
}
void user_key_init(void)
{
    uapi_pin_set_mode(USER_KEY, PIN_MODE_0);

    uapi_pin_set_pull(USER_KEY, PIN_PULL_TYPE_DOWN);
    uapi_gpio_set_dir(USER_KEY, GPIO_DIRECTION_INPUT);
    /* 注册指定GPIO下降沿中断，回调函数为gpio_callback_func */
    if (uapi_gpio_register_isr_func(USER_KEY, GPIO_INTERRUPT_RISING_EDGE, user_key_callback_func) != 0) {
        printf("register_isr_func fail!\r\n");
        uapi_gpio_unregister_isr_func(USER_KEY);
    }
    uapi_gpio_enable_interrupt(USER_KEY);
}

void oled_show(void)
{
    char test[20] = "BSP TEST!";
    SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_0, test, TEXT_SIZE_16);
    if (sprintf_s(oledShowBuff, sizeof(oledShowBuff), "TEMP:%d", (int)temperature) > 0) {
        SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_1, oledShowBuff, TEXT_SIZE_16);
        memset_s(oledShowBuff, sizeof(oledShowBuff), 0, sizeof(oledShowBuff));
    }
    if (sprintf_s(oledShowBuff, sizeof(oledShowBuff), "NFC:%s", ((nfc_test == 1) ? " OK" : "FAI")) > 0) {
        SSD1306_ShowStr(OLED_TEXT16_COLUMN_0 + 65, OLED_TEXT16_LINE_1, oledShowBuff, TEXT_SIZE_16);
        memset_s(oledShowBuff, sizeof(oledShowBuff), 0, sizeof(oledShowBuff));
    }
    if (sprintf_s(oledShowBuff, sizeof(oledShowBuff), "IR:%04d", (int)ir) > 0) {
        SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_2, oledShowBuff, TEXT_SIZE_16);
        memset_s(oledShowBuff, sizeof(oledShowBuff), 0, sizeof(oledShowBuff));
    }
    if (sprintf_s(oledShowBuff, sizeof(oledShowBuff), "ALS: %04d", (int)als) > 0) {
        SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_3, oledShowBuff, TEXT_SIZE_16);
        memset_s(oledShowBuff, sizeof(oledShowBuff), 0, sizeof(oledShowBuff));
    }
    if (sprintf_s(oledShowBuff, sizeof(oledShowBuff), "PS:%04d", (int)ps) > 0) {
        SSD1306_ShowStr(OLED_TEXT16_COLUMN_0 + 65, OLED_TEXT16_LINE_2, oledShowBuff, TEXT_SIZE_16);
        memset_s(oledShowBuff, sizeof(oledShowBuff), 0, sizeof(oledShowBuff));
    }
}
