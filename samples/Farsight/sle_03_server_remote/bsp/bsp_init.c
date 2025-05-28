#include "bsp_init.h"
#include "hal_bsp_dc.h"
#include "stdio.h"
#include "sle_server.h"
#include "pid.h"
uint16_t distance;
uint8_t track;
/*串口接收缓冲区大小 */
#define UART_RX_MAX 512
uint8_t uart_rx_buffer[UART_RX_MAX];

/* 串口接收io */
#define CONFIG_UART_TXD_PIN 8
#define CONFIG_UART_RXD_PIN 7
#define CONFIG_UART_PIN_MODE 2
/* 传感器 */
uint16_t L_Motor_PWM;
uint16_t R_Motor_PWM;
uint16_t Motor_PWM[3] = {40, 55, 70};
#define COEFFICIENT_1000 1000.0
system_value_t systemValue = {0}; // 系统全局变量

// OLED显示屏显示任务
char oledShowBuff[50] = {0};
void user_key_callback_func(pin_t pin, uintptr_t param)
{
    unused(pin);
    unused(param);
    // 开/关 寻迹模式
    systemValue.car_speed = 0;
    tmp_io.bit.p3 = 0; // 打开串口控制
    PCF8574_Write(tmp_io.all);
    systemValue.auto_track_flag = !systemValue.auto_track_flag;
}
void user_key_init(void)
{
    uapi_pin_set_mode(USER_KEY, PIN_MODE_0);

    uapi_pin_set_mode(USER_KEY, 0);
    uapi_pin_set_pull(USER_KEY, PIN_PULL_TYPE_DOWN);
    uapi_gpio_set_dir(USER_KEY, GPIO_DIRECTION_INPUT);
    /* 注册指定GPIO下降沿中断，回调函数为gpio_callback_func */
    if (uapi_gpio_register_isr_func(USER_KEY, GPIO_INTERRUPT_RISING_EDGE, user_key_callback_func) != 0) {
        printf("register_isr_func fail!\r\n");
        uapi_gpio_unregister_isr_func(USER_KEY);
    }
    uapi_gpio_enable_interrupt(USER_KEY);
}
void user_beep_init(void)
{
    uapi_pin_set_mode(USER_BEEP, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(USER_BEEP, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(USER_BEEP, GPIO_LEVEL_LOW);
}
// 切换小车的工作状态
char *get_CurrentCarStatus(system_value_t sys)
{
    char *data = NULL;
    switch (sys.car_status) {
        case CAR_STATUS_RUN:
            data = "run  ";
            break;
        case CAR_STATUS_BACK:
            data = "back ";
            break;
        case CAR_STATUS_LEFT:
            data = "left ";
            break;
        case CAR_STATUS_RIGHT:
            data = "right";
            break;
        case CAR_STATUS_STOP:
            data = "stop ";
            break;
        default:
            data = "     ";
            break;
    }

    return data;
}
// 屏幕显示
void oled_show(void)
{
    memset_s((char *)oledShowBuff, sizeof(oledShowBuff), 0, sizeof(oledShowBuff));
    if (sprintf((char *)oledShowBuff, "sle_sate: %s", (g_sle_conn_flag == 1 ? "  ok" : "disc")) > 0) {
        SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_0, oledShowBuff, TEXT_SIZE_16);
    }
    memset_s((char *)oledShowBuff, sizeof(oledShowBuff), 0, sizeof(oledShowBuff));
    if (sprintf((char *)oledShowBuff, "power: %.1fV", (float)(systemValue.battery_voltage) / COEFFICIENT_1000) > 0) {
        SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_1, oledShowBuff, TEXT_SIZE_16);
    }
    /* 显示小车当前的状态 */
    memset_s((char *)oledShowBuff, sizeof(oledShowBuff), 0, sizeof(oledShowBuff));
    if (sprintf((char *)oledShowBuff, "track: %s", (systemValue.auto_track_flag == 1 ? " on" : "off")) > 0) {
        SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_2, oledShowBuff, TEXT_SIZE_16);
    }
    /* 显示小车当前的速度 */
    memset_s((char *)oledShowBuff, sizeof(oledShowBuff), 0, sizeof(oledShowBuff));
    if (systemValue.car_speed == 0) {
        sprintf((char *)oledShowBuff, "speed: low   ");
    } else if (systemValue.car_speed == 1) {
        sprintf((char *)oledShowBuff, "speed: middle");
    } else if (systemValue.car_speed == 2) {
        sprintf((char *)oledShowBuff, "speed: high  ");
    }
    SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_3, oledShowBuff, TEXT_SIZE_16);
}

void car_control_data(system_value_t sys)
{

    switch (sys.car_status) {
        case CAR_STATUS_RUN:

            car_advance(L_Motor_PWM, R_Motor_PWM);
            systemValue.car_status = CAR_STATUS_RUN;
            break;
        case CAR_STATUS_BACK:
            car_back(L_Motor_PWM, R_Motor_PWM);
            systemValue.car_status = CAR_STATUS_BACK;
            break;
        case CAR_STATUS_LEFT:
            car_turn_left();
            systemValue.car_status = CAR_STATUS_LEFT;
            break;
        case CAR_STATUS_RIGHT:
            car_turn_right();
            systemValue.car_status = CAR_STATUS_RIGHT;
            break;
        case CAR_STATUS_STOP:
            car_stop();
            systemValue.car_status = CAR_STATUS_STOP;
            break;
        default:
            car_stop();
            systemValue.car_status = CAR_STATUS_STOP;
            break;
    }
}
char uart2_recv[30];
uint8_t recv_flag;
/* 串口接收回调 */
void uart_read_handler(const void *buffer, uint16_t length, bool error)
{
    unused(error);
    memcpy_s(uart2_recv, length, buffer, length);

    if (uart2_recv[0] == 0xff)
        systemValue.distance = uart2_recv[1] * 256 + uart2_recv[2];
    else if (uart2_recv[0] == 0xaa)
        systemValue.track_data = uart2_recv[1];
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