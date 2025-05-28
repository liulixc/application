#include "bsp_init.h"
#include "stdio.h"
#include "hal_bsp_bmps.h"

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
    sys_msg_data.smartControl_flag = !sys_msg_data.smartControl_flag;
}

/**
 * @brief  按键初始化函数
 * @note   并且有中断回调函数
 * @retval None
 */
void KEY_Init(void)
{
    uapi_pin_set_mode(KEY, PIN_MODE_0);

    // uapi_pin_set_mode(USER_KEY, 0);
    uapi_pin_set_pull(KEY, PIN_PULL_TYPE_DOWN);
    uapi_gpio_set_dir(KEY, GPIO_DIRECTION_INPUT);
    /* 注册指定GPIO下降沿中断，回调函数为gpio_callback_func */
    if (uapi_gpio_register_isr_func(KEY, GPIO_INTERRUPT_RISING_EDGE, user_key_callback_func) != 0) {
        printf("register_isr_func fail!\r\n");
        uapi_gpio_unregister_isr_func(KEY);
    }
    uapi_gpio_enable_interrupt(KEY);
}

void lcd_display(void)
{
    char lcd_display_buf[100];
    memset(lcd_display_buf, 0, sizeof(lcd_display_buf));
    sprintf(lcd_display_buf, "temp:");
    LCD_ShowString(68, 61, strlen(lcd_display_buf), 16, (uint8_t *)lcd_display_buf);
    memset(lcd_display_buf, 0, sizeof(lcd_display_buf));
    sprintf(lcd_display_buf, "%.1f", sys_msg_data.temperature);
    LCD_ShowString(68, 77, strlen(lcd_display_buf), 16, (uint8_t *)lcd_display_buf);
    memset(lcd_display_buf, 0, sizeof(lcd_display_buf));

    sprintf(lcd_display_buf, "humi:");
    LCD_ShowString(68, 98, strlen(lcd_display_buf), 16, (uint8_t *)lcd_display_buf);
    memset(lcd_display_buf, 0, sizeof(lcd_display_buf));
    sprintf(lcd_display_buf, "%.1f", sys_msg_data.humidity);
    LCD_ShowString(68, 114, strlen(lcd_display_buf), 16, (uint8_t *)lcd_display_buf);
    memset(lcd_display_buf, 0, sizeof(lcd_display_buf));

    sprintf(lcd_display_buf, "fan:");
    LCD_ShowString(68, 143, strlen(lcd_display_buf), 16, (uint8_t *)lcd_display_buf);
    memset(lcd_display_buf, 0, sizeof(lcd_display_buf));
    sprintf(lcd_display_buf, "%s", sys_msg_data.fanStatus == 1 ? " ON" : "OFF");
    LCD_ShowString(68, 159, strlen(lcd_display_buf), 16, (uint8_t *)lcd_display_buf);
    memset(lcd_display_buf, 0, sizeof(lcd_display_buf));

    sprintf(lcd_display_buf, "peop:");
    LCD_ShowString(174, 61, strlen(lcd_display_buf), 16, (uint8_t *)lcd_display_buf);
    memset(lcd_display_buf, 0, sizeof(lcd_display_buf));
    sprintf(lcd_display_buf, "%s", sys_msg_data.is_Body ? " ON" : "OFF");
    LCD_ShowString(174, 77, strlen(lcd_display_buf), 16, (uint8_t *)lcd_display_buf);
    memset(lcd_display_buf, 0, sizeof(lcd_display_buf));

    sprintf(lcd_display_buf, "beep:");
    LCD_ShowString(174, 143, strlen(lcd_display_buf), 16, (uint8_t *)lcd_display_buf);
    memset(lcd_display_buf, 0, sizeof(lcd_display_buf));
    sprintf(lcd_display_buf, "%s", uapi_gpio_get_val(USER_BEEP) ? " ON" : "OFF");
    LCD_ShowString(174, 159, strlen(lcd_display_buf), 16, (uint8_t *)lcd_display_buf);
    memset(lcd_display_buf, 0, sizeof(lcd_display_buf));

    sprintf(lcd_display_buf, "lamp:");
    LCD_ShowString(280, 61, strlen(lcd_display_buf), 16, (uint8_t *)lcd_display_buf);
    memset(lcd_display_buf, 0, sizeof(lcd_display_buf));
    sprintf(lcd_display_buf, "%s", uapi_gpio_get_val(USER_LED) ? "OFF" : " ON");
    LCD_ShowString(280, 85, strlen(lcd_display_buf), 16, (uint8_t *)lcd_display_buf);

    memset(lcd_display_buf, 0, sizeof(lcd_display_buf));
    sprintf(lcd_display_buf, "temp:%02d  ps:%4d  als:%4d", sys_msg_data.threshold_value.temp_threshold_value,
            sys_msg_data.threshold_value.ps_threshold_value, sys_msg_data.threshold_value.als_threshold_value);
    LCD_ShowString(0, 212, strlen(lcd_display_buf), 24, (uint8_t *)lcd_display_buf);
    if (sys_msg_data.fanStatus) {
        LCD_DrawPicture(0, 143, 68, 211, (uint8_t *)gImage_fanon);

    } else {
        LCD_DrawPicture(0, 143, 68, 211, (uint8_t *)gImage_fanoff);
    }

    if (sys_msg_data.is_Body) {
        LCD_DrawPicture(106, 61, 174, 129, (uint8_t *)gImage_rention);

    } else {
        LCD_DrawPicture(106, 61, 174, 129, (uint8_t *)gImage_rentioff);
    }

    if (uapi_gpio_get_val(USER_BEEP)) {
        LCD_DrawPicture(106, 143, 174, 211, (uint8_t *)gImage_beepon);

    } else {
        LCD_DrawPicture(106, 143, 174, 211, (uint8_t *)gImage_beepoff);
    }

    if (uapi_gpio_get_val(USER_LED)) {
        LCD_DrawPicture(212, 61, 280, 129, (uint8_t *)gImage_lampoff);

    } else {
        LCD_DrawPicture(212, 61, 280, 129, (uint8_t *)gImage_lampon);
    }

    if (sys_msg_data.smartControl_flag) {
        LCD_DrawPicture(20, 0, 64, 44, (uint8_t *)gImage_auto);
    } else {
        LCD_DrawPicture(20, 0, 64, 44, (uint8_t *)gImage_no_auto);
    }
}