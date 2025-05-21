#include "hal_bsp_dc.h"
#include "bsp_init.h"
uint32_t left_encoder_temp, right_encoder_temp; // 左电机和右电机的编码器脉冲计数值 - 临时值
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
void user_m1_fan(void)
{
    uapi_pin_set_mode(IN0_M1, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(IN0_M1, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(IN0_M1, GPIO_LEVEL_LOW);

    uapi_pin_set_mode(IN1_M1, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(IN1_M1, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(IN1_M1, GPIO_LEVEL_HIGH);
}
// 反转
void user_m1_zheng(void)
{
    uapi_pin_set_mode(IN0_M1, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(IN0_M1, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(IN0_M1, GPIO_LEVEL_HIGH);

    uapi_pin_set_mode(IN1_M1, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(IN1_M1, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(IN1_M1, GPIO_LEVEL_LOW);
}
// 正转
void user_m2_fan(void)
{
    uapi_pin_set_mode(IN0_M2, PIN_MODE_4);
    uapi_gpio_set_dir(IN0_M2, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(IN0_M2, GPIO_LEVEL_LOW);

    uapi_pin_set_mode(IN1_M2, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(IN1_M2, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(IN1_M2, GPIO_LEVEL_HIGH);
}
// 反转
void user_m2_zheng(void)
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
    // uapi_pwm_deinit();
}
// 前进
void car_advance(uint32_t l_pwm, uint32_t r_pwm)
{
    user_pwm_change(l_pwm, r_pwm);
    user_m1_zheng();
    user_m2_zheng();
}
// 后退
void car_back(uint32_t l_pwm, uint32_t r_pwm)
{
    user_pwm_change(l_pwm, r_pwm);
    user_m1_fan();
    user_m2_fan();
}
void car_turn_left(void)
{
    user_pwm_change(20000, 20000);
    user_m1_fan();
    user_m2_zheng();
}
void car_turn_right(void)
{
    user_pwm_change(20000, 20000);
    user_m2_fan();
    user_m1_zheng();
}

// pwm引脚初始化
void user_pwm_init(void)
{
    pwm_config_t PWM_level = {10000, 30000, 0, 0, true};
    uapi_pin_set_mode(USER_PWM0, PIN_MODE_1);
    uapi_pin_set_mode(USER_PWM1, PIN_MODE_1);

    uapi_pwm_init();
    uapi_pwm_open(PWM_CHANNEL0, &PWM_level);
    uapi_pwm_open(PWM_CHANNEL1, &PWM_level);

    uint8_t channel_id = PWM_CHANNEL0;
    uapi_pwm_set_group(1, &channel_id, 1);
    uapi_pwm_start(PWM_CHANNEL0);

    channel_id = PWM_CHANNEL1;
    uapi_pwm_set_group(2, &channel_id, 1);
    uapi_pwm_start(PWM_CHANNEL1);
}

void user_pwm_change(uint32_t l_pwm, uint32_t r_pwm)
{
    uint32_t l_temp = 40000 - l_pwm;
    uint32_t r_temp = 40000 - r_pwm;

    pwm_config_t PWM_level[2] = {
        {l_temp, l_pwm, 0, 0, true},
        {r_temp, r_pwm, 0, 0, true},
    };
    uapi_pwm_init();
    uapi_pwm_open(PWM_CHANNEL0, &PWM_level[0]);
    uapi_pwm_open(PWM_CHANNEL1, &PWM_level[1]);

    uint8_t channel_id = PWM_CHANNEL0;
    uapi_pwm_set_group(1, &channel_id, 1);
    uapi_pwm_start(PWM_CHANNEL0);

    channel_id = PWM_CHANNEL1;
    uapi_pwm_set_group(2, &channel_id, 1);
    uapi_pwm_start(PWM_CHANNEL1);
}

void m1_enc_callback_func(pin_t pin, uintptr_t param)
{
    unused(pin);
    unused(param);
    left_encoder_temp++;
}
void m2_enc_callback_func(pin_t pin, uintptr_t param)
{
    unused(pin);
    unused(param);
    right_encoder_temp++;
}
void motor_enc_init(void)
{
    uapi_pin_set_mode(M1_ENC, PIN_MODE_0);
    uapi_pin_set_pull(M1_ENC, PIN_PULL_TYPE_DOWN);
    uapi_gpio_set_dir(M1_ENC, GPIO_DIRECTION_INPUT);

    uapi_pin_set_mode(M2_ENC, PIN_MODE_0);
    uapi_pin_set_pull(M2_ENC, PIN_PULL_TYPE_DOWN);
    uapi_gpio_set_dir(M2_ENC, GPIO_DIRECTION_INPUT);

    /* 注册指定GPIO上升沿中断，回调函数为gpio_callback_func */
    if (uapi_gpio_register_isr_func(M1_ENC, GPIO_INTERRUPT_RISING_EDGE, m1_enc_callback_func) != 0) {
        printf("register_isr_func fail!\r\n");
        uapi_gpio_unregister_isr_func(M1_ENC);
    }
    uapi_gpio_enable_interrupt(M1_ENC);

    if (uapi_gpio_register_isr_func(M2_ENC, GPIO_INTERRUPT_RISING_EDGE, m2_enc_callback_func) != 0) {
        printf("register_isr_func fail!\r\n");
        uapi_gpio_unregister_isr_func(M2_ENC);
    }
    uapi_gpio_enable_interrupt(M2_ENC);
}