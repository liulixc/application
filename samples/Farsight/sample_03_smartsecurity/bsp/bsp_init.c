#include "bsp_init.h"
#include "stdio.h"

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
void set_buzzer(uint8_t status)
{
    if (status == true) {
        uapi_gpio_set_val(USER_BEEP, GPIO_LEVEL_HIGH); // 蜂鸣器开启
    } else if (status == false) {
        uapi_gpio_set_val(USER_BEEP, GPIO_LEVEL_LOW); // 蜂鸣器关闭
    }
}
