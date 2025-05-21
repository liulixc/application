
#ifndef HAL_BSP_IR_H
#define HAL_BSP_IR_H
#include "osal_debug.h"
#include "osal_task.h"
#include "securec.h"
#include "gpio.h"
#include "pinctrl.h"
#include "timer.h"
#include "osal_addr.h"
#include "osal_wait.h"
#include "bsp_init.h"
#define IR_IO GPIO_13
void ir_timer_init(void);
void user_ir_init(void);

#define Remote_Infrared_DAT_INPUT HAL_GPIO_ReadPin(GPIOF, GPIO_PIN_15)

typedef struct _Remote_Infrared_data_struct // 定义红外线接收到的数据结构体类型
{
    uint8_t bKeyCodeNot; // 按键码反码
    uint8_t bKeyCode;    // 按键码
    uint8_t bIDNot;      // 用户码反码
    uint8_t bID;         // 用户码
} Remote_Infrared_data_struct;

typedef union _Remote_Infrared_data_union // 定义红外线接收到的数据结构体类型
{
    Remote_Infrared_data_struct RemoteInfraredDataStruct;
    uint32_t uiRemoteInfraredData;
} Remote_Infrared_data_union;

void Remote_Infrared_KEY_ISR(void);
uint8_t Remote_Infrared_KeyDeCode(void);
#endif