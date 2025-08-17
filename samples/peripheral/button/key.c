/**

 *
 * Description: 按键中断示例程序
 * 本文件演示了如何使用GPIO中断来处理按键事件，采用中断驱动方式而非轮询方式
 */

#include "stdio.h"
#include "string.h"
#include "soc_osal.h"
#include "securec.h"
#include "osal_debug.h"
#include "cmsis_os2.h"
#include "app_init.h"
#include "cmsis_os2.h"
#include "stdio.h"
#include "pinctrl.h"
#include "gpio.h"
#include "i2c.h"
#include "securec.h"

osThreadId_t task1_ID; // 任务1

#define DELAY_TIME_MS 10
#define INT_IO GPIO_14
#define INT_IO1 GPIO_01
#define INT_IO2 GPIO_02
#define INT_IO3 GPIO_03

uint8_t int_flag;
void press_callback_func(pin_t pin, uintptr_t param)
{
    unused(pin);
    unused(param);
    printf("pin:%d",pin);
    printf("press_callback_func!\r\n");
    osal_msleep(30);
    int_flag = pin;
}
void user_key_init(void)
{
    /* 配置引脚为下拉，并设置为输入模式 */
    uapi_pin_set_mode(INT_IO, PIN_MODE_0);
    uapi_pin_set_pull(INT_IO, PIN_PULL_TYPE_UP);
    uapi_gpio_set_dir(INT_IO, GPIO_DIRECTION_INPUT);

    uapi_pin_set_mode(INT_IO1, PIN_MODE_0);
    uapi_pin_set_pull(INT_IO1, PIN_PULL_TYPE_UP);
    uapi_gpio_set_dir(INT_IO1, GPIO_DIRECTION_INPUT);

    uapi_pin_set_mode(INT_IO2, PIN_MODE_0);
    uapi_pin_set_pull(INT_IO2, PIN_PULL_TYPE_UP);
    uapi_gpio_set_dir(INT_IO2, GPIO_DIRECTION_INPUT);

    uapi_pin_set_mode(INT_IO3, PIN_MODE_0);
    uapi_pin_set_pull(INT_IO3, PIN_PULL_TYPE_UP);
    uapi_gpio_set_dir(INT_IO3, GPIO_DIRECTION_INPUT);
    
    /* 注册指定GPIO上升沿中断，回调函数为gpio_callback_func */
    int_flag = 1;

    if (uapi_gpio_register_isr_func(INT_IO, GPIO_INTERRUPT_FALLING_EDGE, press_callback_func) != 0) {
        printf("register_isr_func fail!\r\n");
        uapi_gpio_unregister_isr_func(INT_IO);
    }

    if (uapi_gpio_register_isr_func(INT_IO1, GPIO_INTERRUPT_FALLING_EDGE, press_callback_func) != 0) {
        printf("register_isr_func fail!\r\n");
        uapi_gpio_unregister_isr_func(INT_IO1);
    }
    if (uapi_gpio_register_isr_func(INT_IO2, GPIO_INTERRUPT_FALLING_EDGE, press_callback_func) != 0) {
        printf("register_isr_func fail!\r\n");
        uapi_gpio_unregister_isr_func(INT_IO2);
    }
    if (uapi_gpio_register_isr_func(INT_IO3, GPIO_INTERRUPT_FALLING_EDGE, press_callback_func) != 0) {
        printf("register_isr_func fail!\r\n");
        uapi_gpio_unregister_isr_func(INT_IO3);
    }

    /* 使能中断 */
    uapi_gpio_enable_interrupt(INT_IO);
    uapi_gpio_enable_interrupt(INT_IO1);
    uapi_gpio_enable_interrupt(INT_IO2);
    uapi_gpio_enable_interrupt(INT_IO3);

}
void task1(void)
{
    // 初始化按键中断引脚
    user_key_init();
    while (1) {
        // printf("wait");
        // 如果发生中断，则读取按键键值
        if (int_flag) {
            switch (init_flag)
            {
            case 1:
                /* code */
                break;
            case 2:
                /* code */
                break;

            case 3:
                /* code */
                break;

            case 14:
                /* code */
                break;
            

            default:
                break;
            }
        }

        osDelay(DELAY_TIME_MS);
    }
}
static void base_sc12b_demo(void)
{
    printf("Enter base_sc12b_demo()!\r\n");

    osThreadAttr_t attr;
    attr.name = "task1";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 0x2000;
    attr.priority = osPriorityNormal;

    task1_ID = osThreadNew((osThreadFunc_t)task1, NULL, &attr);
    if (task1_ID != NULL) {
        printf("ID = %d, Create task1_ID is OK!\r\n", task1_ID);
    }
}
app_run(base_sc12b_demo);