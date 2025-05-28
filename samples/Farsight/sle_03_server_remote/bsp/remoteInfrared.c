#include "remoteInfrared.h"
#include "pid.h"
/************************************************************************
-------------------------红外接收协议--------------------------
开始拉低9ms,接着是一个4.5ms的高脉冲,通知器件开始传送数据了
接着是发送4个8位二进制码,第一二个是遥控识别码(REMOTE_ID),第一个为
正码(0),第二个为反码(255),接着两个数据是键值,第一个为正码
第二个为反码.发送完后40ms,遥控再发送一个9ms低,2ms高的脉冲,
表示按键的次数,出现一次则证明只按下了一次,如果出现多次,则可
以认为是持续按下该键.

*名称: Remote_Infrared_KEY_ISR(INT11_vect )
*功能: INT0中断服务程序
*参数: 无
*返回: 无
*************************************************************************/
uint32_t GlobalTimingDelay100us;

uint32_t FlagGotKey = 0;

Remote_Infrared_data_union RemoteInfrareddata;

#define RELOAD_CNT 100
#define TIMER_IRQ_PRIO 3 /* 中断优先级范围，从高到低: 0~7 */

timer_handle_t timer1_handle = 0;
void timer1_callback(uintptr_t data)
{
    unused(data);
    if (GlobalTimingDelay100us != 0) {
        GlobalTimingDelay100us--;
    }

    /* 开启下一次timer中断 */
    uapi_timer_start(timer1_handle, RELOAD_CNT, timer1_callback, 0);
}
void ir_timer_init(void)
{
    uapi_timer_init();
    /* 设置 timer1 硬件初始化，设置中断号，配置优先级 */
    uapi_timer_adapter(TIMER_INDEX_1, TIMER_1_IRQN, TIMER_IRQ_PRIO);
    /* 创建 timer1 软件定时器控制句柄 */
    uapi_timer_create(TIMER_INDEX_1, &timer1_handle);
    uapi_timer_start(timer1_handle, RELOAD_CNT, timer1_callback, 0);
}

// 当触发中断时的函数
void ir_callback_func(pin_t pin, uintptr_t param)
{
    unused(pin);
    unused(param);
    Remote_Infrared_KEY_ISR();
}
void user_ir_init(void)
{
    uapi_pin_set_mode(IR_IO, PIN_MODE_0);
    uapi_pin_set_pull(IR_IO, PIN_PULL_TYPE_UP);
    uapi_gpio_set_dir(IR_IO, GPIO_DIRECTION_INPUT);

    /* 注册指定GPIO双边沿中断，回调函数为gpio_callback_func */
    if (uapi_gpio_register_isr_func(IR_IO, GPIO_INTERRUPT_DEDGE, ir_callback_func) != 0) {
        printf("register_isr_func fail!\r\n");
        uapi_gpio_unregister_isr_func(IR_IO);
    }
    uapi_gpio_enable_interrupt(IR_IO);
}

// 检测脉冲宽度最长脉宽为5ms
const uint32_t TIME_DELAY_6MS = 60;
const uint32_t TIME_DELAY_10MS = 100;
void Remote_Infrared_KEY_ISR(void)
{
    static uint8_t bBitCounter = 0; // 键盘帧位计数
    static uint32_t bKeyCode = 0;
    bBitCounter++;
    // printf("  %d\n", bBitCounter);

    if (bBitCounter == 1) // 开始拉低9ms
    {
        if (uapi_gpio_get_val(IR_IO)) // 高电平无效
        {
            bBitCounter = 0;
        } else {
            GlobalTimingDelay100us = TIME_DELAY_10MS;
        }
    } else if (bBitCounter == 2) // 4.5ms的高脉冲
    {
        if (uapi_gpio_get_val(IR_IO)) {
            if ((GlobalTimingDelay100us > 2) && (GlobalTimingDelay100us < 20)) {
                GlobalTimingDelay100us = TIME_DELAY_6MS;
            } else {
                bBitCounter = 0;
                // printf(".");
            }
        }

        else {
            bBitCounter = 0;
        }
    } else if (bBitCounter == 3) {
        if (uapi_gpio_get_val(IR_IO)) {
            bBitCounter = 0;
        } else {
            if ((GlobalTimingDelay100us > 5) && (GlobalTimingDelay100us < 20)) {
                GlobalTimingDelay100us = TIME_DELAY_6MS;
                // printf("引导码");
            } else if ((GlobalTimingDelay100us > 32) && (GlobalTimingDelay100us < 46)) {
                bBitCounter = 0;
                RemoteInfrareddata.uiRemoteInfraredData = bKeyCode;

                bBitCounter = 0;
                FlagGotKey = 1;
            } else {
                bBitCounter = 0;
            }
        }
    } else if (bBitCounter > 3 && bBitCounter < 68) // ????8λ????
    {
        // 2再进这里将时间重置
        if (uapi_gpio_get_val(IR_IO)) {
            if ((GlobalTimingDelay100us > 50) && (GlobalTimingDelay100us < 59)) {
                GlobalTimingDelay100us = TIME_DELAY_6MS;
            } else {
                bBitCounter = 0;
            }
        }
        // 1进来信号变成低电平，先进这里
        else {

            // 3再进这里，判断是0是1，再重置时间
            if ((GlobalTimingDelay100us > 52) && (GlobalTimingDelay100us < 59)) // '0'
            {
                GlobalTimingDelay100us = TIME_DELAY_6MS;
                bKeyCode <<= 1; // MSB First
                bKeyCode += 0x00;

            } else if ((GlobalTimingDelay100us > 40) && (GlobalTimingDelay100us < 53)) //'1'
            {
                GlobalTimingDelay100us = TIME_DELAY_6MS;
                bKeyCode <<= 1; // MSB First
                bKeyCode += 0x01;

            } else {
                bBitCounter = 0;
            }
        }

        if (bBitCounter == 67) {
            RemoteInfrareddata.uiRemoteInfraredData = bKeyCode;
            bBitCounter = 0;
            FlagGotKey = 1;
        }
    } else {
        bBitCounter = 0;
    }
}

/************************************************************************
 *名称: unsigned char Remote_Infrared_KeyDeCode(unsigned char bKeyCode)
 *功能: PS2键盘解码程序
 *参数: bKeyCode 按键码
 *返回: 按键的 按键码
 ************************************************************************/
uint8_t Remote_Infrared_KeyDeCode(void)
{
    uint8_t temp1 = ~RemoteInfrareddata.RemoteInfraredDataStruct.bIDNot;
    uint8_t temp2 = ~RemoteInfrareddata.RemoteInfraredDataStruct.bKeyCodeNot;
    if (FlagGotKey == 1) // 通码
    {
        FlagGotKey = 0;
        if ((RemoteInfrareddata.RemoteInfraredDataStruct.bID == temp1) &&
            (RemoteInfrareddata.RemoteInfraredDataStruct.bKeyCode == temp2)) {
            printf("用户码：%x\t用户码反码: %x\t按键码: %x\t按键码反码: %x\n",
                   (RemoteInfrareddata.uiRemoteInfraredData >> 24) & 0xFF,
                   (RemoteInfrareddata.uiRemoteInfraredData >> 16) & 0xFF,
                   (RemoteInfrareddata.uiRemoteInfraredData >> 8) & 0xFF,
                   (RemoteInfrareddata.uiRemoteInfraredData & 0xFF));

            switch (RemoteInfrareddata.RemoteInfraredDataStruct.bKeyCode) {
                case 0xC2:
                    systemValue.car_status = CAR_STATUS_RUN;
                    break;
                case 0x50:
                    systemValue.car_status = CAR_STATUS_BACK;
                    break;
                case 0x60:
                    systemValue.car_status = CAR_STATUS_LEFT;
                    break;
                case 0x70:
                    systemValue.car_status = CAR_STATUS_RIGHT;
                    break;
                case 0x40:
                    systemValue.car_status = CAR_STATUS_STOP;
                    break;
                case 0x1A:
                    if (systemValue.car_speed >= 1) {
                        systemValue.car_speed -= 1;
                    }
                    break;
                case 0xD8:
                    if (systemValue.car_speed < 2) {
                        systemValue.car_speed += 1;
                    }
                    break;
                case 0xF2:
                    printf("星闪控制模式\n");
                    break;
                case 0xB2:
                    // 关闭寻迹模式
                    systemValue.auto_track_flag = 0;
                    tmp_io.bit.p3 = 1;
                    PCF8574_Write(tmp_io.all);
                    break;
                case 0x72:
                    printf("CAR_HOME\n");
                    break;
                case 0xEA:
                    systemValue.car_status = CAR_STATUS_STOP;
                    break;
                case 0x58:
                    // 开启寻迹模式
                    systemValue.car_speed = 0;
                    systemValue.auto_track_flag = 1;
                    tmp_io.bit.p3 = 0;
                    PCF8574_Write(tmp_io.all);
                    break;
                default:
                    systemValue.car_status = CAR_STATUS_STOP;
                    break;
            }
        } else {
            printf("\n\r ERR 0x%08X", RemoteInfrareddata.uiRemoteInfraredData);
        }
    }

    return RemoteInfrareddata.RemoteInfraredDataStruct.bKeyCode;
}
