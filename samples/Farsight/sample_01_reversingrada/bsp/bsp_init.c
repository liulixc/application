#include "bsp_init.h"
#include "stdio.h"
/*串口接收缓冲区大小*/
#define UART_RX_MAX 512
uint8_t uart_rx_buffer[UART_RX_MAX];
/* 串口接收io*/
#define CONFIG_UART_TXD_PIN 8
#define CONFIG_UART_RXD_PIN 7
#define CONFIG_UART_PIN_MODE 2

// OLED显示屏显示任务
char oledShowBuff[50] = {0};

char uart2_recv[30];
uint16_t distance;
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

/* 串口接收回调 */
void uart_read_handler(const void *buffer, uint16_t length, bool error)
{
    unused(error);
    memcpy_s(uart2_recv, length, buffer, length);
    if (uart2_recv[0] == 0xff)
        distance = uart2_recv[1] * 256 + uart2_recv[2];
}
// /* 串口初始化配置*/
void app_uart_init_config(void)
{
    uart_buffer_config_t uart_buffer_config;
    uapi_pin_set_mode(CONFIG_UART_TXD_PIN, CONFIG_UART_PIN_MODE);
    uapi_pin_set_mode(CONFIG_UART_RXD_PIN, CONFIG_UART_PIN_MODE);
    uart_attr_t attr = {
        .baud_rate = 9600, .data_bits = UART_DATA_BIT_8, .stop_bits = UART_STOP_BIT_1, .parity = UART_PARITY_NONE};
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