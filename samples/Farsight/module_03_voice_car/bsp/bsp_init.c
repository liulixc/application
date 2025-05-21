#include "bsp_init.h"
#include "stdio.h"
float temperature = 0;
float humidity = 0;
/*串口接收缓冲区大小*/
#define UART_RX_MAX 512
uint8_t uart_rx_buffer[UART_RX_MAX];
/* 串口接收io*/
#define CONFIG_UART_TXD_PIN 8
#define CONFIG_UART_RXD_PIN 7
#define CONFIG_UART_PIN_MODE 2

#define COEFFICIENT_1000 1000.0
system_value_t systemValue = {0}; // 系统全局变量
uart_recv uart2_recv = {0};
// OLED显示屏显示任务
char oledShowBuff[50] = {0};

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
    /* 显示小车当前的速度 */
    memset_s((char *)oledShowBuff, sizeof(oledShowBuff), 0, sizeof(oledShowBuff));
    if (sprintf((char *)oledShowBuff, "speed_level: %d", dcspeed_flag) > 0) {
        SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_3, oledShowBuff, TEXT_SIZE_16);
    }
}
// 解析语音模块包
void voice_control_data(uint8_t *pstr)
{

    uint8_t cmd[6] = {0xAA, 0X55, 0X01, 0X00, 0x55, 0xAA};
    switch (pstr[0]) {
        case 0x02: // 获取温湿度
            cmd[3] = (uint8_t)temperature;
            uart_send_buff(cmd, sizeof(cmd));
            break;
        case 0x03: // 前进
            car_advance();
            systemValue.car_status = CAR_STATUS_RUN;
            osal_msleep(1000);
            car_stop();
            systemValue.car_status = CAR_STATUS_STOP;
            break;
        case 0x04: // 后退
            car_back();
            systemValue.car_status = CAR_STATUS_BACK;
            osal_msleep(1000);
            car_stop();
            systemValue.car_status = CAR_STATUS_STOP;
            break;
        case 0x05: // 左转
            if (dcspeed_flag == 0)
                car_turn_left(380);
            else if (dcspeed_flag == 1)
                car_turn_left(60);
            else if (dcspeed_flag == 2)
                car_turn_left(60);
            systemValue.car_status = CAR_STATUS_LEFT;
            break;
        case 0x06: // 右转
            if (dcspeed_flag == 0)
                car_turn_right(380);
            else if (dcspeed_flag == 1)
                car_turn_right(60);
            else if (dcspeed_flag == 2)
                car_turn_right(60);
            systemValue.car_status = CAR_STATUS_RIGHT;
            break;
        case 0x07: // 停车
            car_stop();
            systemValue.car_status = CAR_STATUS_STOP;
            break;
        case 0x09: // 打开蜂鸣器
            uapi_gpio_set_val(USER_BEEP, GPIO_LEVEL_HIGH);
            break;
        case 0x0A: // 关闭蜂鸣器
            uapi_gpio_set_val(USER_BEEP, GPIO_LEVEL_LOW);
            break;
        case 0x0B: // 加速
            if (dcspeed_flag < 2) {
                dcspeed_flag++;
                // 重新配置占空比
                printf("speed:%d\r\n", dcspeed_flag);
                user_pwm_change(dcspeed_flag);
            } else {
                dcspeed_flag = 2;
                printf("已是最高档位\n");
            }
            break;
        case 0x0C: // 减速
            if (dcspeed_flag > 0) {
                dcspeed_flag--;
                // 重新配置占空比
                printf("speed:%d\r\n", dcspeed_flag);
                user_pwm_change(dcspeed_flag);
            } else {
                dcspeed_flag = 0;
                printf("已是最低档位\n");
            }
            break;
        default:
            break;
    }
}
// 发送数据包
uint32_t uart_send_buff(uint8_t *str, uint16_t len)
{
    uint32_t ret = 0;
    ret = uapi_uart_write(UART_ID, str, len, 0xffffffff);
    if (ret != 0) {
        printf("send lenth:%d\n", ret);
    }
    return ret;
}
// 读取队列中接收到的回复 并打印
void recv_uart_hex(void)
{
    printf("recv:");
    for (uint16_t k = 0; k < uart2_recv.recv_len; k++) {
        /* code */
        printf("%X ", uart2_recv.recv[k]);
    }
    printf("\r\n");
    // 解析数据包
    voice_control_data(uart2_recv.recv);
    uart2_recv.recv_flag = 0;
}

/* 串口接收回调 */
void uart_read_handler(const void *buffer, uint16_t length, bool error)
{
    unused(error);
    uart2_recv.recv_len = 0;
    memset(uart2_recv.recv, 0, sizeof(uart2_recv.recv));
    memcpy_s(uart2_recv.recv, length, buffer, length);
    uart2_recv.recv_len = length;
    uart2_recv.recv_flag = 1;
}

/* 串口初始化配置*/
void app_uart_init_config(void)
{
    uart_buffer_config_t uart_buffer_config;
    uapi_pin_set_mode(CONFIG_UART_TXD_PIN, CONFIG_UART_PIN_MODE);
    uapi_pin_set_mode(CONFIG_UART_RXD_PIN, CONFIG_UART_PIN_MODE);
    uart_attr_t attr = {
        .baud_rate = 115200, .data_bits = UART_DATA_BIT_8, .stop_bits = UART_STOP_BIT_1, .parity = UART_PARITY_NONE};
    uart_buffer_config.rx_buffer_size = 512;
    uart_buffer_config.rx_buffer = uart_rx_buffer;
    uart_pin_config_t pin_config = {.tx_pin = S_MGPIO0, .rx_pin = S_MGPIO1, .cts_pin = PIN_NONE, .rts_pin = PIN_NONE};
    uapi_uart_deinit(UART_ID);
    int res = uapi_uart_init(UART_ID, &pin_config, &attr, NULL, &uart_buffer_config);
    if (res != 0) {
        printf("uart init failed res = %02x\r\n", res);
    }
    if (uapi_uart_register_rx_callback(UART_ID, UART_RX_CONDITION_MASK_IDLE, 1, uart_read_handler) == ERRCODE_SUCC) {
        osal_printk("uart%d int mode register receive callback succ!\r\n", UART_BUS_0);
    }
}