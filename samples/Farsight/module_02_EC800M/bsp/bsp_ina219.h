
#ifndef BSP_INA219_H__
#define BSP_INA219_H__

#include <stdbool.h>
#include "stdio.h"
#include "pinctrl.h"
#include "gpio.h"
#include "i2c.h"
#include "osal_task.h"
#include "securec.h"
#include "cmsis_os2.h"

#define INA219_I2C_ADDR 0x41    // 器件的I2C从机地址
#define INA219_I2C_IDX 1        // 模块的I2C总线号
#define INA219_I2C_SPEED 100000 // 100KHz

// 获取电压值
uint16_t INA219_get_bus_voltage_mv(uint16_t *busvolt);

// 获取电流值
uint16_t INA219_get_current_ma(void);

// 获取电源功率值
uint16_t INA219_get_power_mw(void);
// 初始化函数
uint32_t INA219_Init(void);

#endif
