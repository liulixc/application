#include "bsp_init.h"
#include "stdio.h"
#include "hal_bsp_ec800m.h"

float temperature = 0;
float humidity = 0;
uint8_t latitude[20] = {0};
uint8_t longitude[20] = {0};
/*串口接收缓冲区大小*/
uart_recv uart2_recv = {0};
/* 串口接收io*/
#define CONFIG_UART_TXD_PIN 8
#define CONFIG_UART_RXD_PIN 7
#define CONFIG_UART_PIN_MODE 2

// OLED显示屏显示
char oledShowBuff[50] = {0};
system_value_t systemValue = {0}; // 系统全局变量
void user_led_init(void)
{
    uapi_pin_set_mode(USER_LED, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(USER_LED, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(USER_LED, GPIO_LEVEL_HIGH);
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
void control_data(system_value_t sys)
{
    switch (sys.car_status) {
        case 1: // 前进
            car_advance();
            osal_msleep(2000);
            car_stop();
            systemValue.car_status = CAR_STATUS_STOP;
            break;
        case 2: // 后退
            car_back();
            osal_msleep(2000);
            car_stop();
            systemValue.car_status = CAR_STATUS_STOP;
            break;
        case 3: // 左转
            car_turn_left(60);
            systemValue.car_status = CAR_STATUS_STOP;
            break;
        case 4: // 右转
            car_turn_right(60);
            systemValue.car_status = CAR_STATUS_STOP;
            break;
        case 5: // 停车
            car_stop();
            systemValue.car_status = CAR_STATUS_STOP;
            break;
        default:
            break;
    }
}
// 屏幕显示
void oled_show(void)
{
    /* 显示当前温度 */
    if (sprintf(oledShowBuff, "temp: %.1f", temperature) > 0) {
        SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_0, oledShowBuff, TEXT_SIZE_16);
        memset_s(oledShowBuff, sizeof(oledShowBuff), 0, sizeof(oledShowBuff));
    }
    memset_s((char *)oledShowBuff, sizeof(oledShowBuff), 0, sizeof(oledShowBuff));
    if (sprintf((char *)oledShowBuff, "power: %.1fV", (float)(systemValue.battery_voltage) / COEFFICIENT_1000) > 0) {
        SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_1, oledShowBuff, TEXT_SIZE_16);
    }
    /* 显示小车当前的状态 */
    memset_s((char *)oledShowBuff, sizeof(oledShowBuff), 0, sizeof(oledShowBuff));
    if (sprintf((char *)oledShowBuff, "car: %s", get_CurrentCarStatus(systemValue)) > 0) {
        SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_2, oledShowBuff, TEXT_SIZE_16);
    }
}

// 发送数据包
uint32_t uart_send_buff(uint8_t *str, uint16_t len)
{
    uint32_t ret = 0;
    // printf("[AT SEND ]: %s\n", str);
    ret = uapi_uart_write(UART_ID, str, len, 0xffffffff);
    if (ret != 0) {
        return ret;
    }
    return ret;
}

/* 串口初始化配置*/
void app_uart_init_config(void)
{
    uart_buffer_config_t g_app_uart_buffer_config = {.rx_buffer = uart2_recv.recv, .rx_buffer_size = UART_RECV_SIZE};
    uapi_pin_set_mode(CONFIG_UART_TXD_PIN, CONFIG_UART_PIN_MODE);
    uapi_pin_set_mode(CONFIG_UART_RXD_PIN, CONFIG_UART_PIN_MODE);
    uart_attr_t attr = {
        .baud_rate = 115200, .data_bits = UART_DATA_BIT_8, .stop_bits = UART_STOP_BIT_1, .parity = UART_PARITY_NONE};

    uart_pin_config_t pin_config = {.tx_pin = S_MGPIO0, .rx_pin = S_MGPIO1, .cts_pin = PIN_NONE, .rts_pin = PIN_NONE};
    uapi_uart_deinit(UART_ID);
    int ret = uapi_uart_init(UART_ID, &pin_config, &attr, NULL, &g_app_uart_buffer_config);
    if (ret != 0) {
        printf("uart init failed ret = %02x\n", ret);
    }
}
