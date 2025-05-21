#include "bsp_init.h"
#include "stdio.h"
#include "hal_bsp_ssd1306.h"

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

void user_pwm_init(void)
{
    pwm_config_t PWM_LOW = {200, // 低电平
                            10,  // 高电平
                            0, 0, true};
    uapi_pin_set_mode(USER_PWM, PIN_MODE_1);
    uapi_pwm_init();
    uapi_pwm_open(PWM_CHANNEL, &PWM_LOW);

    uint8_t channel_id = PWM_CHANNEL;
    uapi_pwm_set_group(1, &channel_id, 1);
    uapi_pwm_start(PWM_CHANNEL);
}

char *get_CurrentRadarPattern(system_value_t sys)
{
    char *data = NULL;
    switch (sys.radar_status) {
        case Radar_STATUS_ON:
            data = "ON ";
            break;
        case Radar_STATUS_OFF:
            data = "OFF";
            break;
        default:
            data = "    ";
            break;
    }

    return data;
}
// oled显示函数
void oled_show(void)
{
    char test[20] = "Smart trashcan!";
    ssd1306_show_str(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_0, test, TEXT_SIZE_16);
    if (sprintf_s(oledShowBuff, sizeof(oledShowBuff), "State: %s", get_CurrentRadarPattern(systemdata)) > 0) {
        ssd1306_show_str(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_1, oledShowBuff, TEXT_SIZE_16);
        memset_s(oledShowBuff, sizeof(oledShowBuff), 0, sizeof(oledShowBuff));
    }
    if (sprintf_s(oledShowBuff, sizeof(oledShowBuff), "Distance: %3dcm", systemdata.distance)) {
        ssd1306_show_str(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_2, oledShowBuff, TEXT_SIZE_16);
        memset_s(oledShowBuff, sizeof(oledShowBuff), 0, sizeof(oledShowBuff));
    }
}
// 雷达控制函数
void rado_control_data(uint8_t *pstr)
{
    uint8_t cmd_on[7] = {0xAA, 0XAA, 0X87, 0X21, 0x00, 0x55, 0x55};  // 打开捅盖命令
    uint8_t cmd_off[7] = {0xAA, 0XAA, 0X00, 0X21, 0x00, 0x55, 0x55}; // 关闭捅盖命令
    uint16_t buff_sum = pstr[3] + (pstr[4] << 8);                    // 计算距离
    systemdata.distance = buff_sum;
    if (((pstr[3] + (pstr[4] << 8)) < 0x32)) // 判断是否有人并且人体距离雷达小于50cm
    {
        systemdata.radar_status = Radar_STATUS_ON; // 改变桶盖的状态
        uart_send_buff(cmd_on, sizeof(cmd_on));    // 发送命令
    } else {
        systemdata.radar_status = Radar_STATUS_OFF;
        uart_send_buff(cmd_off, sizeof(cmd_off));
    }
}
// 读取队列中接收到的回复 并打印
void recv_uart_hex(void)
{
    uint8_t recv_buf[20] = {0};
    uint8_t j = 0;
    uint16_t recv_len = uapi_uart_read(UART_ID, recv_buf, sizeof(recv_buf), 0); // 接收雷达模块数据包
    if (recv_len == 0)
        return;
    while (1) // 得到一包有效数据
    {
        if (recv_buf[j] == 0xaa && recv_buf[j + 1] == 0xaa) {
            break;
        }
        j++;
    }
    printf("pack:");
    for (uint16_t k = 0; k < 7; k++) {
        /* code */
        printf("%X ", recv_buf[j + k]);
    }
    printf("\r\n");

    // 解析数据包
    rado_control_data((recv_buf + j));
    memset(recv_buf, 0, sizeof(recv_buf));
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

/* 串口初始化配置*/
void app_uart_init_config(void)
{
    uart_buffer_config_t uart_buffer_config;
    uapi_pin_set_mode(CONFIG_UART_TXD_PIN, CONFIG_UART_PIN_MODE);
    uapi_pin_set_mode(CONFIG_UART_RXD_PIN, CONFIG_UART_PIN_MODE);
    uart_attr_t attr = {
        .baud_rate = 115200, .data_bits = UART_DATA_BIT_8, .stop_bits = UART_STOP_BIT_1, .parity = UART_PARITY_NONE};
    uart_buffer_config.rx_buffer_size = UART_RX_MAX;
    uart_buffer_config.rx_buffer = uart_rx_buffer;
    uart_pin_config_t pin_config = {
        .tx_pin = CONFIG_UART_TXD_PIN, .rx_pin = CONFIG_UART_RXD_PIN, .cts_pin = PIN_NONE, .rts_pin = PIN_NONE};
    uapi_uart_deinit(UART_ID);
    int res = uapi_uart_init(UART_ID, &pin_config, &attr, NULL, &uart_buffer_config);
    if (res != 0) {
        printf("uart init failed res = %02x\r\n", res);
    }
}
