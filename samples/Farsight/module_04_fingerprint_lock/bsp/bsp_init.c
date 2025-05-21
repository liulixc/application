#include "bsp_init.h"
#include "stdio.h"
#include "hal_bsp_ssd1306.h"
#include "hal_bsp_aw2013.h"

/* 消息队列结构体 */
unsigned long g_msg_queue = 0;
unsigned int msg_rev_size = sizeof(msg_data_t);
/*串口接收缓冲区大小*/
#define UART_RX_MAX 512
uint8_t uart_rx_buffer[UART_RX_MAX];
/* 串口接收io*/
#define CONFIG_UART_TXD_PIN 8
#define CONFIG_UART_RXD_PIN 7
#define CONFIG_UART_PIN_MODE 2

// OLED显示屏显示任务
char oledShowBuff[50] = {0};
system_value_t systemdata; // 系统全局变量
void user_led_init(void)
{
    uapi_pin_set_mode(USER_LED, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(USER_LED, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(USER_LED, GPIO_LEVEL_HIGH);
}
void user_key_callback_func(pin_t pin, uintptr_t param)
{
    unused(pin);
    unused(param);
    if (systemdata.key_pattern) {
        systemdata.key_pattern = KEY_PATTERN_INPUT;
    } else {
        systemdata.key_pattern = KEY_PATTERN_TEST;
    }
}
void user_key_init(void)
{
    uapi_pin_set_mode(USER_KEY, PIN_MODE_0);

    // uapi_pin_set_mode(USER_KEY, 0);
    uapi_pin_set_pull(USER_KEY, PIN_PULL_TYPE_DOWN);
    uapi_gpio_set_dir(USER_KEY, GPIO_DIRECTION_INPUT);
    /* 注册指定GPIO下降沿中断，回调函数为gpio_callback_func */
    if (uapi_gpio_register_isr_func(USER_KEY, GPIO_INTERRUPT_RISING_EDGE, user_key_callback_func) != 0) {
        printf("register_isr_func fail!\r\n");
        uapi_gpio_unregister_isr_func(USER_KEY);
    }
    uapi_gpio_enable_interrupt(USER_KEY);
}
void finger_io_init(void)
{
#ifdef DEIVER_BOARD
    /* 指纹触摸 */
    uapi_pin_set_mode(FINGER_KEY, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(FINGER_KEY, GPIO_DIRECTION_INPUT);
    /* 电磁锁 */
    uapi_pin_set_mode(FINGER_LOCK, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(FINGER_LOCK, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(FINGER_LOCK, GPIO_LEVEL_LOW);
#endif
    /* 蜂鸣器 */
    uapi_pin_set_mode(BEEP, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(BEEP, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(BEEP, GPIO_LEVEL_LOW);
    /* 首先设置为测试模式 */
    systemdata.key_pattern = 1;
}
// 切换指纹锁的模式
char *get_CurrentKeyPattern(system_value_t sys)
{
    char *data = NULL;
    switch (sys.key_pattern) {
        case KEY_PATTERN_TEST:
            data = "TEST ";
            break;
        case KEY_PATTERN_INPUT:
            data = "INPUT";
            break;
        default:
            data = "ERROR";
            break;
    }
    return data;
}

// 切换指纹按键的状态
char *get_CurrentKeyStatus(system_value_t sys)
{
    char *data = NULL;
    switch (sys.key_status) {
        case STATUS_ON:
            data = "ON ";
            break;
        case STATUS_OFF:
            data = "OFF";
            break;
        default:
            data = "ERR";
            break;
    }
    return data;
}
// 切换指纹按键的状态
char *get_CurrentlockStatus(system_value_t sys)
{
    char *data = NULL;
    switch (sys.lock_status) {
        case STATUS_ON:
            data = "ON ";
            break;
        case STATUS_OFF:
            data = "OFF";
            break;
    }
    return data;
}
void oled_show(void)
{

    char test[20] = "Finger_Test!";
    ssd1306_show_str(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_0, test, TEXT_SIZE_16);

    sprintf_s(oledShowBuff, sizeof(oledShowBuff), "Mode: %s", get_CurrentKeyPattern(systemdata));
    ssd1306_show_str(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_1, oledShowBuff, TEXT_SIZE_16);
    memset_s(oledShowBuff, sizeof(oledShowBuff), 0, sizeof(oledShowBuff));

    sprintf_s(oledShowBuff, sizeof(oledShowBuff), "Key: %s", get_CurrentKeyStatus(systemdata));
    ssd1306_show_str(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_2, oledShowBuff, TEXT_SIZE_16);
    memset_s(oledShowBuff, sizeof(oledShowBuff), 0, sizeof(oledShowBuff));

    sprintf_s(oledShowBuff, sizeof(oledShowBuff), "Lock: %s", get_CurrentlockStatus(systemdata));
    ssd1306_show_str(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_3, oledShowBuff, TEXT_SIZE_16);
    memset_s(oledShowBuff, sizeof(oledShowBuff), 0, sizeof(oledShowBuff));
}
// 读取队列中接收到的回复 并打印
void recv_finger_hex(msg_data_t *recv)
{
    int msg_ret = osal_msg_queue_read_copy(g_msg_queue, recv, &msg_rev_size, OSAL_WAIT_FOREVER);
    if (msg_ret != 0) {
        printf("msg queue read copy fail.");
        if (recv->value != NULL) {
            osal_vfree(recv->value);
        }
    }
    // printf("recv:");
    // for (uint16_t i = 0; i < recv->value_len; i++) {
    //     /* code */
    //     printf("%x ", recv->value[i]);
    // }
    // printf("\r\n");
}
// 发送数据包
uint32_t uart_send_buff(uint8_t *str, uint16_t len)
{
    uint32_t ret = 0;
    ret = uapi_uart_write(UART_BUS_2, str, len, 0xffffffff);
    // if (ret != 0) {
    //     printf("send lenth:%d\n", ret);
    // }
    return ret;
}
/* 串口接收回调 */
void uart_read_handler(const void *buffer, uint16_t length, bool error)
{
    unused(error);
    msg_data_t msg_data = {0};
    void *buffer_cpy = osal_vmalloc(length);
    if (memcpy_s(buffer_cpy, length, buffer, length) != EOK) {
        osal_vfree(buffer_cpy);
        return;
    }
    msg_data.value = (uint8_t *)buffer_cpy;
    msg_data.value_len = length;
    osal_msg_queue_write_copy(g_msg_queue, (void *)&msg_data, msg_rev_size, 0);
    osal_vfree(buffer_cpy);
}
// /* 串口初始化配置*/
void app_uart_init_config(void)
{
    uart_buffer_config_t uart_buffer_config;
    uapi_pin_set_mode(CONFIG_UART_TXD_PIN, CONFIG_UART_PIN_MODE);
    uapi_pin_set_mode(CONFIG_UART_RXD_PIN, CONFIG_UART_PIN_MODE);
    uart_attr_t attr = {
        .baud_rate = 57600, .data_bits = UART_DATA_BIT_8, .stop_bits = UART_STOP_BIT_1, .parity = UART_PARITY_NONE};
    uart_buffer_config.rx_buffer_size = UART_RX_MAX;
    uart_buffer_config.rx_buffer = uart_rx_buffer;
    uart_pin_config_t pin_config = {
        .tx_pin = CONFIG_UART_TXD_PIN, .rx_pin = CONFIG_UART_RXD_PIN, .cts_pin = PIN_NONE, .rts_pin = PIN_NONE};
    uapi_uart_deinit(UART_ID);
    int res = uapi_uart_init(UART_ID, &pin_config, &attr, NULL, &uart_buffer_config);
    if (res != 0) {
        printf("uart init failed res = %02x\r\n", res);
    }
    if (uapi_uart_register_rx_callback(UART_ID, UART_RX_CONDITION_MASK_IDLE, 1, uart_read_handler) == ERRCODE_SUCC) {
        printf("uart%d int mode register receive callback succ!\r\n", UART_ID);
    }
}
