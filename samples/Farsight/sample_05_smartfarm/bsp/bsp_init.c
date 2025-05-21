#include "bsp_init.h"
#include "stdio.h"
#include "hal_bsp_bmps.h"
void user_fan_init(void)
{
    uapi_pin_set_mode(USER_FAN, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(USER_FAN, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(USER_FAN, GPIO_LEVEL_HIGH);
}
void set_fan(uint8_t status)
{

    if (status)
        status = 0;
    else
        status = 1;
    uapi_gpio_set_val(USER_FAN, status);
}
void user_key_callback_func(pin_t pin, uintptr_t param)
{
    unused(pin);
    unused(param);
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
static uint8_t fan_gif_index = 0;
#define FAN_GIF_INDEX_MAX 3

margin_t bmp_number_1 = {
    .top = 16 + 8,
    .left = 8,
    .width = 16,
    .hight = 32,
}; // 数字-十位
margin_t bmp_number_2 = {
    .top = 16 + 8,
    .left = 24,
    .width = 16,
    .hight = 32,
}; // 数字-个位
margin_t bmp_dian = {
    .top = 32 + 8,
    .left = 40,
    .width = 16,
    .hight = 16,
}; // 小数点
margin_t bmp_number_3 = {
    .top = 32 + 8,
    .left = 56,
    .width = 8,
    .hight = 16,
}; // 数字-小数
margin_t bmp_danwei = {
    .top = 16 + 8,
    .left = 52,
    .width = 16,
    .hight = 16,
}; // 单位
margin_t bmp_image = {
    .top = 16,
    .left = 72,
    .width = 48,
    .hight = 48,
}; // 图片

#define TASK_DELAY_TIME (100 * 1000) // us
#define COEFFICIENT_10 10
#define COEFFICIENT_100 100
#define COEFFICIENT_1000 1000

/**
 * @brief  显示湿度页面
 * @note
 * @param  val:
 * @retval None
 */
void show_humi_page(float val)
{
    SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_0, " Smart Farm", TEXT_SIZE_16);

    int x = (val * COEFFICIENT_100) / COEFFICIENT_1000;
    SSD1306_DrawBMP(bmp_number_1.left, bmp_number_1.top, bmp_number_1.width, bmp_number_1.hight,
                    bmp_16X32_number[x]); // 显示数字的十位

    x = ((int)(val * COEFFICIENT_100)) / COEFFICIENT_100 % COEFFICIENT_10;
    SSD1306_DrawBMP(bmp_number_2.left, bmp_number_2.top, bmp_number_1.width, bmp_number_1.hight,
                    bmp_16X32_number[x]); // 显示数字的个位
    SSD1306_DrawBMP(bmp_dian.left, bmp_dian.top, bmp_dian.width, bmp_dian.hight, bmp_16X16_dian); // 显示小数点
    SSD1306_DrawBMP(bmp_danwei.left, bmp_danwei.top, bmp_danwei.width, bmp_danwei.hight,
                    bmp_16X16_baifenhao); // 显示%符号

    x = ((int)(val * COEFFICIENT_100)) / COEFFICIENT_10 % COEFFICIENT_10;
    SSD1306_DrawBMP(bmp_number_3.left, bmp_number_3.top, bmp_number_3.width, bmp_number_3.hight,
                    bmp_8X16_number[x]); // 显示数字的小数位

    // 风扇动态显示
    if (sys_msg_data.fanStatus == 0) {
        SSD1306_DrawBMP(bmp_image.left, bmp_image.top, bmp_image.width, bmp_image.hight,
                        bmp_48X48_fan_gif[0]); // 静态显示
    } else {
        fan_gif_index++;
        if (fan_gif_index > FAN_GIF_INDEX_MAX)
            fan_gif_index = 0;
        SSD1306_DrawBMP(bmp_image.left, bmp_image.top, bmp_image.width, bmp_image.hight,
                        bmp_48X48_fan_gif[fan_gif_index]); // 动态显示
    }
}