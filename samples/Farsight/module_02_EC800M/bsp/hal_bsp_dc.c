#include "hal_bsp_dc.h"
uint8_t dcspeed_flag = 1;
pwm_config_t PWM_level[3] = {
    {250, 550, 0, 0, true},
    {200, 600, 0, 0, true},
    {10, 790, 0, 0, true},
};
// 点击转向控制io
void car_io_init(void)
{
    // 右侧电机
    uapi_pin_set_mode(IN0_M2, PIN_MODE_4);
    uapi_gpio_set_dir(IN0_M2, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(IN0_M2, GPIO_LEVEL_LOW);

    uapi_pin_set_mode(IN1_M2, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(IN1_M2, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(IN1_M2, GPIO_LEVEL_LOW);
    // 左侧电机
    uapi_pin_set_mode(IN0_M1, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(IN0_M1, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(IN0_M1, GPIO_LEVEL_LOW);

    uapi_pin_set_mode(IN1_M1, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(IN1_M1, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(IN1_M1, GPIO_LEVEL_LOW);
    // 左侧测速码盘
    uapi_pin_set_mode(M1_ENC, HAL_PIO_FUNC_GPIO);
    uapi_pin_set_pull(M1_ENC, PIN_PULL_TYPE_UP);
    uapi_gpio_set_dir(M1_ENC, GPIO_DIRECTION_INPUT);
    // 右侧测速码盘
    uapi_pin_set_mode(M2_ENC, HAL_PIO_FUNC_GPIO);
    uapi_pin_set_pull(M2_ENC, PIN_PULL_TYPE_UP);

    uapi_gpio_set_dir(M2_ENC, GPIO_DIRECTION_INPUT);
}
// 正转
void user_m1_inverse(void)
{
    uapi_pin_set_mode(IN0_M1, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(IN0_M1, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(IN0_M1, GPIO_LEVEL_LOW);

    uapi_pin_set_mode(IN1_M1, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(IN1_M1, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(IN1_M1, GPIO_LEVEL_HIGH);
}
// 反转
void user_m1_positive(void)
{
    uapi_pin_set_mode(IN0_M1, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(IN0_M1, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(IN0_M1, GPIO_LEVEL_HIGH);

    uapi_pin_set_mode(IN1_M1, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(IN1_M1, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(IN1_M1, GPIO_LEVEL_LOW);
}
// 正转
void user_m2_inverse(void)
{
    uapi_pin_set_mode(IN0_M2, PIN_MODE_4);
    uapi_gpio_set_dir(IN0_M2, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(IN0_M2, GPIO_LEVEL_LOW);

    uapi_pin_set_mode(IN1_M2, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(IN1_M2, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(IN1_M2, GPIO_LEVEL_HIGH);
}
// 反转
void user_m2_positive(void)
{
    uapi_pin_set_mode(IN0_M2, PIN_MODE_4);
    uapi_gpio_set_dir(IN0_M2, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(IN0_M2, GPIO_LEVEL_HIGH);

    uapi_pin_set_mode(IN1_M2, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(IN1_M2, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(IN1_M2, GPIO_LEVEL_LOW);
}
void car_stop(void)
{
    // 右侧电机
    uapi_pin_set_mode(IN0_M2, PIN_MODE_4);
    uapi_gpio_set_dir(IN0_M2, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(IN0_M2, GPIO_LEVEL_LOW);

    uapi_pin_set_mode(IN1_M2, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(IN1_M2, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(IN1_M2, GPIO_LEVEL_LOW);
    // 左侧电机
    uapi_pin_set_mode(IN0_M1, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(IN0_M1, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(IN0_M1, GPIO_LEVEL_LOW);

    uapi_pin_set_mode(IN1_M1, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(IN1_M1, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(IN1_M1, GPIO_LEVEL_LOW);
}
// 前进
void car_advance(void)
{
    user_m1_positive();
    user_m2_positive();
}
// 后退
void car_back(void)
{
    user_m1_inverse();
    user_m2_inverse();
}
void car_turn_left(uint16_t delay_time)
{
    user_m1_inverse();
    user_m2_positive();
    osal_mdelay(delay_time);
    car_stop();
}
void car_turn_right(uint16_t delay_time)
{
    user_m2_inverse();
    user_m1_positive();
    osal_mdelay(delay_time);
    car_stop();
}

// pwm引脚初始化
void user_pwm_init(void)
{

    uapi_pin_set_mode(USER_PWM0, PIN_MODE_1);
    uapi_pin_set_mode(USER_PWM1, PIN_MODE_1);

    uapi_pwm_init();
    uapi_pwm_open(PWM_CHANNEL0, &PWM_level[2]);
    uapi_pwm_open(PWM_CHANNEL1, &PWM_level[2]);

    uint8_t channel_id = PWM_CHANNEL0;
    uapi_pwm_set_group(1, &channel_id, 1);
    uapi_pwm_start(PWM_CHANNEL0);

    channel_id = PWM_CHANNEL1;
    uapi_pwm_set_group(1, &channel_id, 1);
    uapi_pwm_start(PWM_CHANNEL1);
}