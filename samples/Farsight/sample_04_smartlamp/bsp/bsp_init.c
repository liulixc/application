#include "bsp_init.h"
#include "hal_bsp_aw2013.h"
#include "stdio.h"
#define RGB_ON 255
#define RGB_OFF 0

// 循环闪烁的节拍值
#define BEAT_0 0
#define BEAT_1 1
#define BEAT_2 2
#define BEAT_3 3

#define KEY_DELAY_TIME (100 * 1000) // us
#define LIGHT_SCALE_VAL 4           // 比例值
typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} ts_rgb_val_t;

// 睡眠模式 RGB灯的PWM值
ts_rgb_val_t sleep_mode_rgb_val = {
    .red = 20,
    .green = 20,
    .blue = 20,
};

// 看书模式 RGB灯的PWM值
ts_rgb_val_t readBook_mode_rgb_val = {
    .red = 226,
    .green = 203,
    .blue = 173,
};
// 按键按下时，改变灯光的模式，三种模式进行切换（白光模式、睡眠模式、看书模式）
static Lamp_Status_t lamp_mode = OFF_LAMP;
static uint8_t t_led_blink_status = 0;

void user_led_init(void)
{
    uapi_pin_set_mode(USER_LED, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(USER_LED, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(USER_LED, GPIO_LEVEL_HIGH);
}

extern char oled_display_buff[30];
// 处理RGB灯模式控制
void switch_rgb_mode(Lamp_Status_t mode)
{
    uint8_t r, g, b;
    switch (mode) {
        case SUN_LIGHT_MODE: // 白光模式
            sys_msg_data.RGB_Value.red = sys_msg_data.RGB_Value.green = sys_msg_data.RGB_Value.blue = RGB_ON;
            break;

        case SLEEP_MODE: // 睡眠模式
            sys_msg_data.RGB_Value.red = sleep_mode_rgb_val.red;
            sys_msg_data.RGB_Value.green = sleep_mode_rgb_val.green;
            sys_msg_data.RGB_Value.blue = sleep_mode_rgb_val.blue;
            break;

        case READ_BOOK_MODE: // 看书模式
            sys_msg_data.RGB_Value.red = readBook_mode_rgb_val.red;
            sys_msg_data.RGB_Value.green = readBook_mode_rgb_val.green;
            sys_msg_data.RGB_Value.blue = readBook_mode_rgb_val.blue;
            break;

        case LED_BLINK_MODE:
            // 闪烁模式
            t_led_blink_status++;

            if (t_led_blink_status == BEAT_1) {
                sys_msg_data.RGB_Value.red = RGB_ON;
                sys_msg_data.RGB_Value.green = sys_msg_data.RGB_Value.blue = RGB_OFF;
            } else if (t_led_blink_status == BEAT_2) {
                sys_msg_data.RGB_Value.green = RGB_ON;
                sys_msg_data.RGB_Value.red = sys_msg_data.RGB_Value.blue = RGB_OFF;
            } else if (t_led_blink_status == BEAT_3) {
                sys_msg_data.RGB_Value.blue = RGB_ON;
                sys_msg_data.RGB_Value.red = sys_msg_data.RGB_Value.green = RGB_OFF;
            } else {
                t_led_blink_status = BEAT_0;
            }
            break;

        case SET_RGB_MODE: // 调色模式

            break;
        case AUTO_MODE: // 调色模式
            sys_msg_data.is_auto_light_mode = LIGHT_AUTO_MODE;
            break;
        default: // 关闭灯光
            lamp_mode = OFF_LAMP;
            sys_msg_data.is_auto_light_mode = 0;
            sys_msg_data.Lamp_Status = OFF_LAMP;
            sys_msg_data.RGB_Value.red = RGB_OFF;
            sys_msg_data.RGB_Value.green = RGB_OFF;
            sys_msg_data.RGB_Value.blue = RGB_OFF;
            break;
    }

    // 根据光照强度进行自动调节
    if (sys_msg_data.is_auto_light_mode == LIGHT_AUTO_MODE) {
        // 显示在OLED显示屏上
        sprintf(oled_display_buff, "light:%04d", 1024 - sys_msg_data.AP3216C_Value.light);
        SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_1, oled_display_buff, TEXT_SIZE_16);

        memset(oled_display_buff, 0, sizeof(oled_display_buff));
        sprintf(oled_display_buff, "Lamp:%03d",
                (uint8_t)(((sys_msg_data.AP3216C_Value.light * 100) / LIGHT_SCALE_VAL)) / 255);
        SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_2, oled_display_buff, TEXT_SIZE_16);

        AW2013_Control_RGB((uint8_t)(sys_msg_data.AP3216C_Value.light / LIGHT_SCALE_VAL),
                           (uint8_t)(sys_msg_data.AP3216C_Value.light / LIGHT_SCALE_VAL),
                           (uint8_t)(sys_msg_data.AP3216C_Value.light / LIGHT_SCALE_VAL));
    } else {
        // 手动调节
        r = sys_msg_data.RGB_Value.red;
        g = sys_msg_data.RGB_Value.green;
        b = sys_msg_data.RGB_Value.blue;
        AW2013_Control_RGB(r, g, b);
    }
}

void user_key_callback_func(pin_t pin, uintptr_t param)
{
    unused(pin);
    unused(param);
    lamp_mode++;
    printf("lamp_mode=%d!\r\n", lamp_mode);
    // 记录灯的状态
    sys_msg_data.Lamp_Status = lamp_mode;
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