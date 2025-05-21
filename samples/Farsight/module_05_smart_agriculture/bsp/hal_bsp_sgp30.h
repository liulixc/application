#ifndef __HAL_BSP_SGP30_H__
#define __HAL_BSP_SGP30_H__

#include "cmsis_os2.h"

#define SGP30_I2C_ADDR 0x58    // 器件的I2C从机地址
#define SGP30_I2C_IDX 1        // 模块的I2C总线号
#define SGP30_I2C_SPEED 100000 // 100KHz
#define I2C_MASTER_ADDR 0x0
/* io*/
#define I2C_SCL_MASTER_PIN 16
#define I2C_SDA_MASTER_PIN 15
#define CONFIG_PIN_MODE 2
void SGP30_ReadData(uint16_t *temp, uint16_t *humi);
/**
 * @brief SHT20 初始化
 */
void SGP30_Init(void);

#endif
