/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "math.h"
#include "pinctrl.h"
#include "spi.h"
#include "gpio.h"
#include "i2c.h"
#include "watchdog.h"
#include "osal_debug.h"
#include "soc_osal.h"
#include "app_init.h"
#include "bms_slave.h"
#include "malloc.h"
#include "cJSON_Utils.h"
#include "pinctrl.h"
#include "adc.h"
#include "adc_porting.h"
#include "sle_uuid_server.h"
#include "sle_hybrid.h"
 #include "gpio.h"     // GPIO 操作相关头文件

#define SPI_SLAVE_NUM 1
#define SPI_BUS_CLK 32000000
#define SPI_FREQUENCY 2
#define SPI_CLK_POLARITY 1
#define SPI_CLK_PHASE 1
#define SPI_FRAME_FORMAT 0
#define SPI_FRAME_FORMAT_STANDARD 0
#define SPI_FRAME_SIZE_8 0x1f
#define SPI_TMOD 0
#define SPI_WAIT_CYCLES 0x10
#define CONFIG_SPI_DI_MASTER_PIN 11
#define CONFIG_SPI_DO_MASTER_PIN 9
#define CONFIG_SPI_CLK_MASTER_PIN 7
#define CONFIG_SPI_CS_MASTER_PIN 8
#define CONFIG_SPI_MASTER_PIN_MODE 3
#define CONFIG_SPI_TRANSFER_LEN 2
#define CONFIG_SPI_MASTER_BUS_ID 0
#define CONFIG_I2C_SCL_MASTER_PIN 15
#define CONFIG_I2C_SDA_MASTER_PIN 16
#define CONFIG_I2C_MASTER_PIN_MODE 2
#define I2C_SET_BANDRATE 400000
#define I2C_MASTER_ADDR 0x0

#define SPI_TASK_STACK_SIZE 0x1000
#define SPI_TASK_DURATION_MS 300
#define SPI_TASK_PRIO 17


/****************************************************************************************** */
 /*! 
 
  |MD| Dec  | ADC Conversion Model|
  |--|------|---------------------|
  |01| 1    | Fast 			   	  |
  |10| 2    | Normal 			  |
  |11| 3    | Filtered 		   	  |
*/
#define MD_FAST 1
#define MD_NORMAL 2
#define MD_FILTERED 3


 /*! 
 |CH | Dec  | Channels to convert |
 |---|------|---------------------|
 |000| 0    | All Cells  		  |
 |001| 1    | Cell 1 and Cell 7   |
 |010| 2    | Cell 2 and Cell 8   |
 |011| 3    | Cell 3 and Cell 9   |
 |100| 4    | Cell 4 and Cell 10  |
 |101| 5    | Cell 5 and Cell 11  |
 |110| 6    | Cell 6 and Cell 12  |
*/
#define CELL_CH_ALL 0
#define CELL_CH_1and7 1
#define CELL_CH_2and8 2
#define CELL_CH_3and9 3
#define CELL_CH_4and10 4
#define CELL_CH_5and11 5
#define CELL_CH_6and12 6

/*!

  |CHG | Dec  |Channels to convert   | 
  |----|------|----------------------|
  |000 | 0    | All GPIOS and 2nd Ref| 
  |001 | 1    | GPIO 1 			     |
  |010 | 2    | GPIO 2               |
  |011 | 3    | GPIO 3 			  	 |
  |100 | 4    | GPIO 4 			  	 |
  |101 | 5    | GPIO 5 			 	 |
  |110 | 6    | Vref2  			  	 |
*/

#define AUX_CH_ALL 0
#define AUX_CH_GPIO1 1
#define AUX_CH_GPIO2 2
#define AUX_CH_GPIO3 3
#define AUX_CH_GPIO4 4
#define AUX_CH_GPIO5 5
#define AUX_CH_VREF2 6

//uint8_t CHG = 0; //!< aux channels to be converted
 /*!****************************************************
  \brief Controls if Discharging transitors are enabled
  or disabled during Cell conversions.
  
 |DCP | Discharge Permitted During conversion |
 |----|----------------------------------------|
 |0   | No - discharge is not permitted         |
 |1   | Yes - discharge is permitted           |
 
********************************************************/
#define DCP_DISABLED 0
#define DCP_ENABLED 1


static const unsigned int crc15Table[256] = {0x0,0xc599, 0xceab, 0xb32, 0xd8cf, 0x1d56, 0x1664, 0xd3fd, 0xf407, 0x319e, 0x3aac,  //!<precomputed CRC15 Table
    0xff35, 0x2cc8, 0xe951, 0xe263, 0x27fa, 0xad97, 0x680e, 0x633c, 0xa6a5, 0x7558, 0xb0c1, 
    0xbbf3, 0x7e6a, 0x5990, 0x9c09, 0x973b, 0x52a2, 0x815f, 0x44c6, 0x4ff4, 0x8a6d, 0x5b2e,
    0x9eb7, 0x9585, 0x501c, 0x83e1, 0x4678, 0x4d4a, 0x88d3, 0xaf29, 0x6ab0, 0x6182, 0xa41b,
    0x77e6, 0xb27f, 0xb94d, 0x7cd4, 0xf6b9, 0x3320, 0x3812, 0xfd8b, 0x2e76, 0xebef, 0xe0dd, 
    0x2544, 0x2be, 0xc727, 0xcc15, 0x98c, 0xda71, 0x1fe8, 0x14da, 0xd143, 0xf3c5, 0x365c, 
    0x3d6e, 0xf8f7,0x2b0a, 0xee93, 0xe5a1, 0x2038, 0x7c2, 0xc25b, 0xc969, 0xcf0, 0xdf0d, 
    0x1a94, 0x11a6, 0xd43f, 0x5e52, 0x9bcb, 0x90f9, 0x5560, 0x869d, 0x4304, 0x4836, 0x8daf,
    0xaa55, 0x6fcc, 0x64fe, 0xa167, 0x729a, 0xb703, 0xbc31, 0x79a8, 0xa8eb, 0x6d72, 0x6640,
    0xa3d9, 0x7024, 0xb5bd, 0xbe8f, 0x7b16, 0x5cec, 0x9975, 0x9247, 0x57de, 0x8423, 0x41ba,
    0x4a88, 0x8f11, 0x57c, 0xc0e5, 0xcbd7, 0xe4e, 0xddb3, 0x182a, 0x1318, 0xd681, 0xf17b, 
    0x34e2, 0x3fd0, 0xfa49, 0x29b4, 0xec2d, 0xe71f, 0x2286, 0xa213, 0x678a, 0x6cb8, 0xa921, 
    0x7adc, 0xbf45, 0xb477, 0x71ee, 0x5614, 0x938d, 0x98bf, 0x5d26, 0x8edb, 0x4b42, 0x4070, 
    0x85e9, 0xf84, 0xca1d, 0xc12f, 0x4b6, 0xd74b, 0x12d2, 0x19e0, 0xdc79, 0xfb83, 0x3e1a, 0x3528, 
    0xf0b1, 0x234c, 0xe6d5, 0xede7, 0x287e, 0xf93d, 0x3ca4, 0x3796, 0xf20f, 0x21f2, 0xe46b, 0xef59, 
    0x2ac0, 0xd3a, 0xc8a3, 0xc391, 0x608, 0xd5f5, 0x106c, 0x1b5e, 0xdec7, 0x54aa, 0x9133, 0x9a01, 
    0x5f98, 0x8c65, 0x49fc, 0x42ce, 0x8757, 0xa0ad, 0x6534, 0x6e06, 0xab9f, 0x7862, 0xbdfb, 0xb6c9, 
    0x7350, 0x51d6, 0x944f, 0x9f7d, 0x5ae4, 0x8919, 0x4c80, 0x47b2, 0x822b, 0xa5d1, 0x6048, 0x6b7a, 
    0xaee3, 0x7d1e, 0xb887, 0xb3b5, 0x762c, 0xfc41, 0x39d8, 0x32ea, 0xf773, 0x248e, 0xe117, 0xea25, 
    0x2fbc, 0x846, 0xcddf, 0xc6ed, 0x374, 0xd089, 0x1510, 0x1e22, 0xdbbb, 0xaf8, 0xcf61, 0xc453, 
    0x1ca, 0xd237, 0x17ae, 0x1c9c, 0xd905, 0xfeff, 0x3b66, 0x3054, 0xf5cd, 0x2630, 0xe3a9, 0xe89b, 
    0x2d02, 0xa76f, 0x62f6, 0x69c4, 0xac5d, 0x7fa0, 0xba39, 0xb10b, 0x7492, 0x5368, 0x96f1, 0x9dc3, 
    0x585a, 0x8ba7, 0x4e3e, 0x450c, 0x8095}; 
    


/******************************************************************   SPI   *****************************************************************/
static void app_spi_init_pin(void)
{
    uapi_pin_set_mode(CONFIG_SPI_DI_MASTER_PIN, CONFIG_SPI_MASTER_PIN_MODE);
    uapi_pin_set_mode(CONFIG_SPI_DO_MASTER_PIN, CONFIG_SPI_MASTER_PIN_MODE);
    uapi_pin_set_mode(CONFIG_SPI_CLK_MASTER_PIN, CONFIG_SPI_MASTER_PIN_MODE);
    uapi_pin_set_mode(CONFIG_SPI_CS_MASTER_PIN, 0);
    uapi_gpio_set_dir(CONFIG_SPI_CS_MASTER_PIN, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(CONFIG_SPI_CS_MASTER_PIN, GPIO_LEVEL_HIGH);
}

static void app_i2c_init_pin(void)
{
    uapi_pin_set_mode(CONFIG_I2C_SCL_MASTER_PIN, CONFIG_I2C_MASTER_PIN_MODE);
    uapi_pin_set_mode(CONFIG_I2C_SDA_MASTER_PIN, CONFIG_I2C_MASTER_PIN_MODE);
}

// SPI 读写函数，向指定寄存器写入数据并读取返回值
// 参数：
//   addr      —— 要写入的寄存器地址或命令字节
//   writedata —— 要写入的数据字节
//   rx_data   —— 用于接收 SPI 返回数据的缓冲区，长度为2
// 返回值：
//   返回 SPI 读到的数据（rx_data[1]），如有错误则返回错误码
static uint8_t gyro_spi_write_read(uint8_t addr, uint8_t writedata, uint8_t *rx_data)
{
    uint32_t ret = 0; // 定义返回值变量
    uint8_t tx_data[CONFIG_SPI_TRANSFER_LEN] = {addr, writedata}; // 构造待发送的 SPI 数据包
    spi_xfer_data_t data = {
        .tx_buff = tx_data,      // 发送缓冲区指针
        .tx_bytes = CONFIG_SPI_TRANSFER_LEN, // 发送字节数
        .rx_buff = rx_data,      // 接收缓冲区指针
        .rx_bytes = CONFIG_SPI_TRANSFER_LEN, // 接收字节数
    };
    uapi_gpio_set_val(CONFIG_SPI_CS_MASTER_PIN, GPIO_LEVEL_LOW); // 片选拉低，开始 SPI 通信

    ret = uapi_spi_master_write(CONFIG_SPI_MASTER_BUS_ID, &data, 0xFFFFFFFF); // 发送数据
    if (ret != 0) {
        printf("spi = %0x master send failed\r\n", ret); // 发送失败打印错误
        return ret;
    }
    ret = uapi_spi_master_read(CONFIG_SPI_MASTER_BUS_ID, &data, 0xFFFFFFFF); // 读取数据
    if (ret != 0) {
        printf("spi = %0x master read failed\r\n", ret); // 读取失败打印错误
        return ret;
    }
    for (int i = 0; i < 2; i++) {
        printf("%0x, readbuff[%d] = %0x\r\n", tx_data[0], i, data.rx_buff[i]); // 打印接收到的数据
    }
    uapi_gpio_set_val(CONFIG_SPI_CS_MASTER_PIN, GPIO_LEVEL_HIGH); // 片选拉高，结束 SPI 通信
    return data.rx_buff[1]; // 返回接收到的有效数据
}


// SPI 写数组函数，发送一组数据时CS只拉低一次
// len  —— 要发送的数据字节数
// data —— 待发送的数据数组
void spi_write_array(uint8_t len, uint8_t data[])
{
    spi_xfer_data_t spi_data = {0};
    spi_data.tx_buff = data;
    spi_data.tx_bytes = len;
    spi_data.rx_buff = NULL;
    spi_data.rx_bytes = 0;

    // 不手动控制CS，由驱动自动管理
    //uapi_spi_master_write(CONFIG_SPI_MASTER_BUS_ID, &spi_data, 0xFFFFFFFF);
}




// SPI 写入并读取指定数量字节的函数，先发送 tx_Data，再发送 rx_len 个 0x00，同时接收数据到 rx_data
void spi_write_read(uint8_t tx_Data[], uint8_t tx_len, uint8_t *rx_data, uint8_t rx_len)
{
    uint8_t total_len = tx_len + rx_len;
    uint8_t TX_Buffer[total_len];
    uint8_t RX_Buffer[total_len];
    memcpy(TX_Buffer, tx_Data, tx_len);
    memset(TX_Buffer + tx_len, 0xFF, rx_len); // dummy 字节建议用 0xFF

    spi_xfer_data_t spi_data = {0};
    spi_data.tx_buff = TX_Buffer;
    spi_data.tx_bytes = total_len;
    spi_data.rx_buff = RX_Buffer;
    spi_data.rx_bytes = total_len;

    uapi_spi_master_writeread(CONFIG_SPI_MASTER_BUS_ID, &spi_data, 0xFFFFFFFF);

    if (rx_data) {
        memcpy(rx_data, RX_Buffer + tx_len, rx_len);
    }
}
/*void spi_write_read(uint8_t tx_Data[], uint8_t tx_len, uint8_t *rx_data, uint8_t rx_len)
{
    if (rx_len == 0) {
        // 只写不读，直接发送
        spi_xfer_data_t spi_data = {0};
        spi_data.tx_buff = tx_Data;
        spi_data.tx_bytes = tx_len;
        spi_data.rx_buff = NULL;
        spi_data.rx_bytes = 0;
        uapi_spi_master_writeread(CONFIG_SPI_MASTER_BUS_ID, &spi_data, 0xFFFFFFFF);
        
        return;
    }
    // 需要读数据，先发送 tx_Data，再补 rx_len 个 0x00
    uint8_t total_len = tx_len + rx_len;
    uint8_t TX_Buffer[total_len];
    uint8_t RX_Buffer[total_len];
    memcpy(TX_Buffer, tx_Data, tx_len); // 拷贝要发送的数据
    memset(TX_Buffer + tx_len, 0x00, rx_len); // 补充空字节
    spi_xfer_data_t spi_data = {0};
    spi_data.tx_buff = TX_Buffer;
    spi_data.tx_bytes = total_len;
    spi_data.rx_buff = RX_Buffer;
    spi_data.rx_bytes = total_len;
    uapi_spi_master_writeread(CONFIG_SPI_MASTER_BUS_ID, &spi_data, 0xFFFFFFFF);
    
    // 只取后 rx_len 字节作为有效读数据
    if (rx_data) {
        memcpy(rx_data, RX_Buffer + tx_len, rx_len);
    }
}*/



static void app_spi_master_init_config(void)
{
    spi_attr_t config = {0};
    spi_extra_attr_t ext_config = {0};

    config.is_slave = false;
    config.slave_num = SPI_SLAVE_NUM;
    config.bus_clk = SPI_BUS_CLK;
    config.freq_mhz = SPI_FREQUENCY;
    config.clk_polarity = SPI_CLK_POLARITY;
    config.clk_phase = SPI_CLK_PHASE;
    config.frame_format = SPI_FRAME_FORMAT;
    config.spi_frame_format = HAL_SPI_FRAME_FORMAT_STANDARD;
    // config.frame_size = SPI_FRAME_SIZE_8; // 80001338代表发送数据的格式不对，需要修改配置参数
    config.frame_size = HAL_SPI_FRAME_SIZE_8;
    config.tmod = SPI_TMOD;
    config.sste = 1;

    ext_config.qspi_param.wait_cycles = SPI_WAIT_CYCLES;
    int ret = uapi_spi_init(CONFIG_SPI_MASTER_BUS_ID, &config, &ext_config);
    if (ret != 0) {
        printf("spi init fail %0x\r\n", ret);
    }
}

static void app_i2c_master_init_config(void)
{
    uint32_t baudrate = I2C_SET_BANDRATE;
    uint32_t hscode = I2C_MASTER_ADDR;
    errcode_t ret = uapi_i2c_master_init(1, baudrate, hscode);
    if (ret != 0) {
        printf("i2c init failed, ret = %0x\r\n", ret);
    }
}

/******************************************************************   LTC6804   *****************************************************************/
/*!
  6804 conversion command variables.  
*/
uint8_t ADCV[2]; //!< Cell Voltage conversion command.
uint8_t ADAX[2]; //!< GPIO conversion command.

#define CS_high  uapi_gpio_set_val(CONFIG_SPI_CS_MASTER_PIN, GPIO_LEVEL_HIGH)
#define CS_low   uapi_gpio_set_val(CONFIG_SPI_CS_MASTER_PIN, GPIO_LEVEL_LOW)

static uint8_t TOTAL_IC=1;
uint8_t tx_cfg[6][6];
uint8_t rx_cfg[6][8];


void init_cfg(void)
{
    int i;

    for(i = 0; i<TOTAL_IC;i++)
    {
        tx_cfg[i][0] = 0xBA ;   //GPIO引脚下拉电路关断(bit8~bit4) | 基准保持上电状态(bit3) | SWTEN处于逻辑1(软件定时器势使能) | ADC模式选择为0
        tx_cfg[i][1] = 0x00 ;   
        tx_cfg[i][2] = 0x00 ;   
        tx_cfg[i][3] = 0x00 ;
        tx_cfg[i][4] = 0x00 ;   //DCC2放电
        tx_cfg[i][5] = 0xB0 ;   //放电超时时间选择最长的120min
    }
}


void set_adc(uint8_t MD, //ADC Mode
    uint8_t DCP, //Discharge Permit
    uint8_t CH, //Cell Channels to be measured
    uint8_t CHG //GPIO Channels to be measured
    )
{
uint8_t md_bits;

md_bits = (MD & 0x02) >> 1;
ADCV[0] = md_bits + 0x02;
md_bits = (MD & 0x01) << 7;
ADCV[1] =  md_bits + 0x60 + (DCP<<4) + CH;

md_bits = (MD & 0x02) >> 1;
ADAX[0] = md_bits + 0x04;
md_bits = (MD & 0x01) << 7;
ADAX[1] = md_bits + 0x60 + CHG ;

}


void wakeup_sleep()
{
  CS_low;
  osal_mdelay(1); //Guarantees the LTC6804 will be in standby
  CS_high;
  osal_mdelay(1);
}


void wakeup_idle()
{
  CS_low;
  osal_mdelay(2); //Guarantees the isoSPI will be in ready mode
  CS_high;
  osal_mdelay(2);
}


uint16_t pec15_calc(uint8_t len, //Number of bytes that will be used to calculate a PEC
    uint8_t *data //Array of data that will be used to calculate  a PEC
    )
{
uint16_t remainder,addr;

remainder = 16;//initialize the PEC
for(uint8_t i = 0; i<len;i++) // loops for each byte in data array
{
addr = ((remainder>>7)^data[i])&0xff;//calculate PEC table address 
remainder = (remainder<<8)^crc15Table[addr];
}
return(remainder*2);//The CRC15 has a 0 in the LSB so the remainder must be multiplied by 2
}



void LTC6804_adcv()
{

  uint8_t cmd[4];
  uint16_t cmd_pec;
  
  //1
  cmd[0] = ADCV[0];
  cmd[1] = ADCV[1];
  
  //2
  cmd_pec = pec15_calc(2, ADCV);
  cmd[2] = (uint8_t)(cmd_pec >> 8);
  cmd[3] = (uint8_t)(cmd_pec);
  
  //3
  wakeup_idle ();  //This will guarantee that the LTC6804 isoSPI port is awake. This command can be removed.
  
  //4
  CS_low;
  spi_write_read(cmd, 4, NULL, 0);
  osal_udelay(100);
  CS_high;

}


void LTC6804_wrcfg(uint8_t total_ic, //The number of ICs being written to
    uint8_t config[][6] //A two dimensional array of the configuration data that will be written
    )
{
    const uint8_t BYTES_IN_REG = 6;
    const uint8_t CMD_LEN = 4+(8*total_ic);
    uint8_t *cmd;
    uint16_t cfg_pec;
    uint8_t cmd_index; //command counter

    cmd = (uint8_t *)malloc(CMD_LEN*sizeof(uint8_t));

    //1
    cmd[0] = 0x00;
    cmd[1] = 0x01;
    cmd[2] = 0x3d;
    cmd[3] = 0x6e;

    //2
    cmd_index = 4;
        for (uint8_t current_ic = total_ic; current_ic > 0; current_ic--) 			// executes for each LTC6804 in daisy chain, this loops starts with
    {																				// the last IC on the stack. The first configuration written is
                                                                 // received by the last IC in the daisy chain
                                                                 
        for (uint8_t current_byte = 0; current_byte < BYTES_IN_REG; current_byte++) // executes for each of the 6 bytes in the CFGR register
        {																			// current_byte is the byte counter

            cmd[cmd_index] = config[current_ic-1][current_byte]; 						//adding the config data to the array to be sent 
            cmd_index = cmd_index + 1;                
        }
    //3
    cfg_pec = (uint16_t)pec15_calc(BYTES_IN_REG, &config[current_ic-1][0]);		// calculating the PEC for each ICs configuration register data
    cmd[cmd_index] = (uint8_t)(cfg_pec >> 8);
    cmd[cmd_index + 1] = (uint8_t)cfg_pec;
    cmd_index = cmd_index + 2;
}

//4
wakeup_idle ();	//This will guarantee that the LTC6804 isoSPI port is awake.This command can be removed.
//5
CS_low;
//HAL_SPI_Transmit(&hspi1,cmd,CMD_LEN,HAL_MAX_DELAY);
spi_write_read(cmd, CMD_LEN, NULL, 0); //Sending the command to the LTC6804
osal_udelay(100);
CS_high;
free(cmd);
}



int8_t LTC6804_rdcfg(uint8_t total_ic, //Number of ICs in the system
    uint8_t r_config[][8] //A two dimensional array that the function stores the read configuration data.
    )
{
    const uint8_t BYTES_IN_REG = 8;

    uint8_t cmd[4];
    uint8_t *rx_data;
    int8_t pec_error = 0; 
    uint16_t data_pec;
    uint16_t received_pec;
    int i,j;
    rx_data = (uint8_t *) malloc((8*total_ic)*sizeof(uint8_t));

    //1
    cmd[0] = 0x00;
    cmd[1] = 0x02;
    cmd[2] = 0x2b;
    cmd[3] = 0x0A;

    //2
    //	wakeup_idle (); //This will guarantee that the LTC6804 isoSPI port is awake. This command can be removed.
    //3
    CS_low;
    spi_write_read(cmd, 4, rx_data, (BYTES_IN_REG*total_ic));         //Read the configuration data of all ICs on the daisy chain into 
    osal_udelay(80);
    CS_high;													//rx_data[] array			

    for (uint8_t current_ic = 0; current_ic < total_ic; current_ic++) 			//executes for each LTC6804 in the daisy chain and packs the data
    { 																		//into the r_config array as well as check the received Config data
                                                                                //for any bit errors	
        //4.a																		
        for (uint8_t current_byte = 0; current_byte < BYTES_IN_REG; current_byte++)					
        {
            r_config[current_ic][current_byte] = rx_data[current_byte + (current_ic*BYTES_IN_REG)];
        }
        //4.b
        received_pec = (r_config[current_ic][6]<<8) + r_config[current_ic][7];
        printf("received_pec=%d \n",received_pec);
        data_pec = pec15_calc(6, &r_config[current_ic][0]);
        printf("data_pec=%d \n",data_pec);
        if(received_pec != data_pec)
        {
            pec_error = -1;
        }  
    }
    free(rx_data);
    //5
    return(pec_error);
}




void LTC6804_initialize(void)
{   int i;    
    init_cfg();
	set_adc(MD_NORMAL,DCP_ENABLED,CELL_CH_ALL,AUX_CH_ALL);//ADAX参数配置
    wakeup_sleep();
    osal_mdelay(2);
	wakeup_idle();
	LTC6804_adcv();
    LTC6804_wrcfg(TOTAL_IC,tx_cfg);
	osal_mdelay(2);
    int8_t r = LTC6804_rdcfg(TOTAL_IC,rx_cfg);
    if (r== -1)
    {
        printf("LTC6804_MODULAR INIT NG!\n\r");
    }
    else
    {
        printf("LTC6804_MODULAR INIT OK!\n\r");
    }
	  
}


void LTC6804_rdcv_reg(uint8_t reg, //Determines which cell voltage register is read back
    uint8_t total_ic, //the number of ICs in the
    uint8_t *data //An array of the unparsed cell codes
    )
{
    const uint8_t REG_LEN = 8; //number of bytes in each ICs register + 2 bytes for the PEC
    uint8_t cmd[4];
    uint16_t cmd_pec;

    //1
    if (reg == 1)     //1: RDCVA
    {
        cmd[1] = 0x04;
        cmd[0] = 0x00;
    }
    else if(reg == 2) //2: RDCVB
    {
        cmd[1] = 0x06;
        cmd[0] = 0x00;
    } 
    else if(reg == 3) //3: RDCVC
    {
        cmd[1] = 0x08;
        cmd[0] = 0x00;
    } 
    else if(reg == 4) //4: RDCVD
    {
        cmd[1] = 0x0A;
        cmd[0] = 0x00;
    } 

    //2
    cmd_pec = pec15_calc(2, cmd);
    cmd[2] = (uint8_t)(cmd_pec >> 8);
    cmd[3] = (uint8_t)(cmd_pec); 

    //3
    wakeup_idle (); //This will guarantee that the LTC6804 isoSPI port is awake. This command can be removed.

    //4
    CS_low;
    spi_write_read(cmd,4,data,(REG_LEN*total_ic));
    osal_udelay(80); 
    CS_high;

}


uint8_t LTC6804_rdcv(uint8_t reg, // Controls which cell voltage register is read back.
    uint8_t total_ic, // the number of ICs in the system
    uint16_t cell_codes[][12] // Array of the parsed cell codes
    )
{

    const uint8_t NUM_RX_BYT = 8;
    const uint8_t BYT_IN_REG = 6;
    const uint8_t CELL_IN_REG = 3;
    int i;
    uint8_t *cell_data;
    uint8_t pec_error = 0;
    uint16_t parsed_cell;
    uint16_t received_pec;
    uint16_t data_pec;
    uint8_t data_counter=0; //data counter
    cell_data = (uint8_t *) malloc((NUM_RX_BYT*total_ic)*sizeof(uint8_t));
    //1.a
    if (reg == 0)
    {
        //a.i
        for(uint8_t cell_reg = 1; cell_reg<5; cell_reg++)         			 			//executes once for each of the LTC6804 cell voltage registers
        {
            data_counter = 0;
            LTC6804_rdcv_reg(cell_reg, total_ic,cell_data );								//Reads a single Cell voltage register
            for(i=0;i<8;i++)
            {
                //	printf("%d ",cell_data[i]);	
            }
                //		printf("\r\n ");


            for (uint8_t current_ic = 0 ; current_ic < total_ic; current_ic++) 			// executes for every LTC6804 in the daisy chain
            {																 	  			// current_ic is used as the IC counter

                //a.ii
                for(uint8_t current_cell = 0; current_cell<CELL_IN_REG; current_cell++)	 	// This loop parses the read back data into cell voltages, it 
                {														   		  			// loops once for each of the 3 cell voltage codes in the register 

                    parsed_cell = cell_data[data_counter] + (cell_data[data_counter + 1] << 8);//Each cell code is received as two bytes and is combined to
                                                                                        // create the parsed cell voltage code
                                                                                        
                    cell_codes[current_ic][current_cell  + ((cell_reg - 1) * CELL_IN_REG)] = parsed_cell;
                    data_counter = data_counter + 2;											 //Because cell voltage codes are two bytes the data counter
                                                                                    //must increment by two for each parsed cell code
                }
                //a.iii
                received_pec = (cell_data[data_counter] << 8) + cell_data[data_counter+1]; //The received PEC for the current_ic is transmitted as the 7th and 8th
                                                                                //after the 6 cell voltage data bytes
                data_pec = pec15_calc(BYT_IN_REG, &cell_data[current_ic * NUM_RX_BYT]);
                if(received_pec != data_pec)
                {
                    pec_error = -1;															//The pec_error variable is simply set negative if any PEC errors 
                                                                                //are detected in the serial data
                }
                data_counter=data_counter+2;												//Because the transmitted PEC code is 2 bytes long the data_counter
                                                                                //must be incremented by 2 bytes to point to the next ICs cell voltage data
            }
        }
    }
    //1.b
    else
    {
        //b.i
        LTC6804_rdcv_reg(reg, total_ic,cell_data);
        for (uint8_t current_ic = 0 ; current_ic < total_ic; current_ic++) 				// executes for every LTC6804 in the daisy chain
        {																 	  			// current_ic is used as the IC counter
            //b.ii
            for(uint8_t current_cell = 0; current_cell < CELL_IN_REG; current_cell++)   // This loop parses the read back data into cell voltages, it 
            {														   		  			// loops once for each of the 3 cell voltage codes in the register 

                parsed_cell = cell_data[data_counter] + (cell_data[data_counter+1]<<8); //Each cell code is received as two bytes and is combined to
                                                                                // create the parsed cell voltage code
                                                                                
                cell_codes[current_ic][current_cell + ((reg - 1) * CELL_IN_REG)] = 0x0000FFFF & parsed_cell;
                data_counter= data_counter + 2;     									//Because cell voltage codes are two bytes the data counter
                                                                                //must increment by two for each parsed cell code
            }
            //b.iii
            received_pec = (cell_data[data_counter] << 8 )+ cell_data[data_counter + 1]; //The received PEC for the current_ic is transmitted as the 7th and 8th
                                                                                //after the 6 cell voltage data bytes
            data_pec = pec15_calc(BYT_IN_REG, &cell_data[current_ic * NUM_RX_BYT]);
            if(received_pec != data_pec)
            {
                pec_error = -1;															//The pec_error variable is simply set negative if any PEC errors 
                                                                            //are detected in the serial data
            }
            data_counter= data_counter + 2; 											//Because the transmitted PEC code is 2 bytes long the data_counter
                                                                            //must be incremented by 2 bytes to point to the next ICs cell voltage data
        }
    }

    //2
    free(cell_data);
    return(pec_error);
}


void LTC6804_adax()
{
  uint8_t cmd[4];
  uint16_t cmd_pec;
 
  cmd[0] = ADAX[0];
  cmd[1] = ADAX[1];
  cmd_pec = pec15_calc(2, ADAX);
  cmd[2] = (uint8_t)(cmd_pec >> 8);
  cmd[3] = (uint8_t)(cmd_pec);
 
  wakeup_idle (); //This will guarantee that the LTC6804 isoSPI port is awake. This command can be removed.
  CS_low;
  //spi_write_array(4,cmd);
  spi_write_read(cmd,4,NULL,0);
  osal_udelay(100);
  CS_high;

}


void LTC6804_rdaux_reg(uint8_t reg, //Determines which GPIO voltage register is read back
    uint8_t total_ic, //The number of ICs in the system
    uint8_t *data //Array of the unparsed auxiliary codes 
    )
{
const uint8_t REG_LEN = 8; // number of bytes in the register + 2 bytes for the PEC
uint8_t cmd[4];
uint16_t cmd_pec;

//1
if (reg == 1)			//Read back auxiliary group A
{
cmd[1] = 0x0C;
cmd[0] = 0x00;
}
else if(reg == 2)		//Read back auxiliary group B 
{
cmd[1] = 0x0e;
cmd[0] = 0x00;
} 
else					//Read back auxiliary group A
{
cmd[1] = 0x0C;		
cmd[0] = 0x00;
}
//2
cmd_pec = pec15_calc(2, cmd);
cmd[2] = (uint8_t)(cmd_pec >> 8);
cmd[3] = (uint8_t)(cmd_pec);

//3
wakeup_idle (); //This will guarantee that the LTC6804 isoSPI port is awake, this command can be removed.
//4
CS_low;
spi_write_read(cmd,4,data,(REG_LEN*total_ic));
osal_udelay(100); //Read the GPIO voltage data of all ICs on the daisy chain into the data[] array
CS_high;

}




int8_t LTC6804_rdaux(uint8_t reg, //Determines which GPIO voltage register is read back. 
    uint8_t total_ic,//the number of ICs in the system
    uint16_t aux_codes[][6]//A two dimensional array of the gpio voltage codes.
    )
{


const uint8_t NUM_RX_BYT = 8;
const uint8_t BYT_IN_REG = 6;
const uint8_t GPIO_IN_REG = 3;

uint8_t *data;
uint8_t data_counter = 0; 
int8_t pec_error = 0;
uint16_t parsed_aux;
uint16_t received_pec;
uint16_t data_pec;
data = (uint8_t *) malloc((NUM_RX_BYT*total_ic)*sizeof(uint8_t));
//1.a
if (reg == 0)
{
//a.i
for(uint8_t gpio_reg = 1; gpio_reg<3; gpio_reg++)		 	   		 			//executes once for each of the LTC6804 aux voltage registers
{
data_counter = 0;
LTC6804_rdaux_reg(gpio_reg, total_ic,data);									//Reads the raw auxiliary register data into the data[] array

for (uint8_t current_ic = 0 ; current_ic < total_ic; current_ic++) 			// executes for every LTC6804 in the daisy chain
{																 	  			// current_ic is used as the IC counter

//a.ii
for(uint8_t current_gpio = 0; current_gpio< GPIO_IN_REG; current_gpio++)	// This loop parses the read back data into GPIO voltages, it 
{														   		  			// loops once for each of the 3 gpio voltage codes in the register 

parsed_aux = data[data_counter] + (data[data_counter+1]<<8);              //Each gpio codes is received as two bytes and is combined to
                                                                   // create the parsed gpio voltage code
                                                                   
aux_codes[current_ic][current_gpio +((gpio_reg-1)*GPIO_IN_REG)] = parsed_aux;
data_counter=data_counter+2;												//Because gpio voltage codes are two bytes the data counter
                                                                   //must increment by two for each parsed gpio voltage code

}
//a.iii
received_pec = (data[data_counter]<<8)+ data[data_counter+1]; 				 //The received PEC for the current_ic is transmitted as the 7th and 8th
                                                                    //after the 6 gpio voltage data bytes
data_pec = pec15_calc(BYT_IN_REG, &data[current_ic*NUM_RX_BYT]);
if(received_pec != data_pec)
{
pec_error = -1;															//The pec_error variable is simply set negative if any PEC errors 
                                                                   //are detected in the received serial data
}

data_counter=data_counter+2;												//Because the transmitted PEC code is 2 bytes long the data_counter
                                                                   //must be incremented by 2 bytes to point to the next ICs gpio voltage data
}


}

}
else
{
//b.i
LTC6804_rdaux_reg(reg, total_ic, data);
for (int current_ic = 0 ; current_ic < total_ic; current_ic++) 			  		// executes for every LTC6804 in the daisy chain
{							   									          		// current_ic is used as an IC counter

//b.ii
for(int current_gpio = 0; current_gpio<GPIO_IN_REG; current_gpio++)  	 	// This loop parses the read back data. Loops 
{						 											  		// once for each aux voltage in the register 

parsed_aux = (data[data_counter] + (data[data_counter+1]<<8));    		//Each gpio codes is received as two bytes and is combined to
                                                                   // create the parsed gpio voltage code
aux_codes[current_ic][current_gpio +((reg-1)*GPIO_IN_REG)] = parsed_aux;
data_counter=data_counter+2;									 		//Because gpio voltage codes are two bytes the data counter
                                                                   //must increment by two for each parsed gpio voltage code
}
//b.iii
received_pec = (data[data_counter]<<8) + data[data_counter+1]; 				 //The received PEC for the current_ic is transmitted as the 7th and 8th
                                                                    //after the 6 gpio voltage data bytes
data_pec = pec15_calc(BYT_IN_REG, &data[current_ic*NUM_RX_BYT]);
if(received_pec != data_pec)
{
pec_error = -1;													   		//The pec_error variable is simply set negative if any PEC errors 
                                                                   //are detected in the received serial data
}

data_counter=data_counter+2;												//Because the transmitted PEC code is 2 bytes long the data_counter
                                                                   //must be incremented by 2 bytes to point to the next ICs gpio voltage data
}
}
free(data);
return (pec_error);
}



/*************************** 均衡  ******************************/

uint16_t Max,Min;
uint16_t MAX_mask = 0;
uint16_t MIN_mask = 0;

uint8_t Total_IC=1;
uint16_t cell_codes[1][12];
uint16_t gpiocode[1][6];
uint8_t r,a;

void Get_Cell_Voltage_Max_Min(void)
{
	char i;
	
	Max = cell_codes[0][0];
	Min = cell_codes[0][0];
	for(i=1;i<12;i++)
	{
		if(cell_codes[0][i] > Max)
		{
			Max = cell_codes[0][i];
		}
		if(cell_codes[0][i] < Min)
		{
			Min = cell_codes[0][i];
		}				
	}
}

uint8_t Make_Balance_Decision(int mv)     //是否满足开启均衡的条件   mv 为 压差  可设置
{
	uint8_t balance_switch;
	if( (Max-Min)>mv )//均衡开启条件  
	{
		balance_switch=1;
	}
	else
	{
		balance_switch=0;
	}
	return(balance_switch);
}

void Write_Balance_Commond(uint16_t Mask)
{
	uint8_t cmd[4];
	uint16_t cmd_pec;
    uint8_t balance_config[15][6]={{0xfc,0x52,0x00,0x00,0x00,0x00}};    //前四个字节为开启均衡命令
    //1111 1100
    //0101 0010
	
	//1
	cmd[0] = balance_config[0][0];
	cmd[1] = balance_config[0][1];
	
	cmd_pec = pec15_calc(2, cmd);

	balance_config[0][2] = (uint8_t)(cmd_pec >> 8);
	balance_config[0][3] = (uint8_t)(cmd_pec);
	
	balance_config[0][4] |= (  Mask & 0x00ff );
	balance_config[0][5] |= ( (Mask >> 8) & 0x000f );
	
	LTC6804_wrcfg(1,balance_config);   //开启均衡
}

void  Balance_task(uint16_t mv)    //计算哪个电池需要均衡
{
	char i;
	uint16_t V_mask = 0;	
	uint8_t balance_switch = 0;
	balance_switch = Make_Balance_Decision(mv);    //判断最大最小压差＞300mv  则开启均衡
	for(i=0;i<12;i++)
	{
		if(cell_codes[0][i]==Max)     //输出所有最大值的位置
		{
			MAX_mask = i+1; 
		}	
		if(cell_codes[0][i]==Min)     //输出所有最小值的位置
		{
			MIN_mask = i+1; 
		}			
	}
	V_mask = 0x0001 << (MAX_mask-1);
//	printf("电压最大的编码：%d \r\n",MAX_mask);
//	printf("电压最小的编码：%d \r\n",MIN_mask);
	if(balance_switch)                           //若满足均衡条件
	{
		
		Write_Balance_Commond(V_mask);	  //均衡控制     1为开启，0为关闭  总共12位，
        //printf("Balance Cell:%d \r\n",V_mask);
        printf("Balance Enable\r\n");
	}
	else 
	{
		Write_Balance_Commond(0x0000);	  //均衡控制     1为开启，0为关闭  总共12位，
        printf("Balance Disable\r\n");
	}
}


int MOD_VOL = 0;

uint8_t Get_SOC(void)
{
    int SOC = 0;

    //cell_codes[12] = MOD_VOL;

  if(MOD_VOL >(4180*120)){SOC=100;}
	else if((MOD_VOL >(4169*120))&&(MOD_VOL <(4180*120))){SOC=99;}
	else if((MOD_VOL >(4157*120))&&(MOD_VOL <(4169*120))){SOC=98;}
	else if((MOD_VOL >(4146*120))&&(MOD_VOL <(4157*120))){SOC=97;}
	else if((MOD_VOL >(4134*120))&&(MOD_VOL <(4146*120))){SOC=96;}
	else if((MOD_VOL >(4123*120))&&(MOD_VOL <(4134*120))){SOC=95;}
	else if((MOD_VOL >(4111*120))&&(MOD_VOL <(4123*120))){SOC=94;}
	else if((MOD_VOL >(4099*120))&&(MOD_VOL <(4111*120))){SOC=93;}
	else if((MOD_VOL >(4088*120))&&(MOD_VOL <(4099*120))){SOC=92;}
	else if((MOD_VOL >(4076*120))&&(MOD_VOL <(4088*120))){SOC=91;}
	else if((MOD_VOL >(4063*120))&&(MOD_VOL <(4076*120))){SOC=90;}
	else if((MOD_VOL >(4051*120))&&(MOD_VOL <(4063*120))){SOC=89;}
	else if((MOD_VOL >(4040*120))&&(MOD_VOL <(4051*120))){SOC=88;}
	else if((MOD_VOL >(4028*120))&&(MOD_VOL <(4040*120))){SOC=87;}
	else if((MOD_VOL >(4017*120))&&(MOD_VOL <(4028*120))){SOC=86;}
	else if((MOD_VOL >(4005*120))&&(MOD_VOL <(4017*120))){SOC=85;}
	else if((MOD_VOL >(3994*120))&&(MOD_VOL <(4005*120))){SOC=84;}
	else if((MOD_VOL >(3982*120))&&(MOD_VOL <(3994*120))){SOC=83;}
	else if((MOD_VOL >(3971*120))&&(MOD_VOL <(3982*120))){SOC=82;}
	else if((MOD_VOL >(3960*120))&&(MOD_VOL <(3971*120))){SOC=81;}
	else if((MOD_VOL >(3949*120))&&(MOD_VOL <(3960*120))){SOC=80;}
	else if((MOD_VOL >(3938*120))&&(MOD_VOL <(3949*120))){SOC=79;}
	else if((MOD_VOL >(3927*120))&&(MOD_VOL <(3938*120))){SOC=78;}
	else if((MOD_VOL >(3916*120))&&(MOD_VOL <(3927*120))){SOC=77;}
	else if((MOD_VOL >(3906*120))&&(MOD_VOL <(3916*120))){SOC=76;}
	else if((MOD_VOL >(3895*120))&&(MOD_VOL <(3906*120))){SOC=75;}
	else if((MOD_VOL >(3885*120))&&(MOD_VOL <(3895*120))){SOC=74;}
	else if((MOD_VOL >(3875*120))&&(MOD_VOL <(3885*120))){SOC=73;}
	else if((MOD_VOL >(3865*120))&&(MOD_VOL <(3875*120))){SOC=72;}
	else if((MOD_VOL >(3855*120))&&(MOD_VOL <(3865*120))){SOC=71;}
	else if((MOD_VOL >(3845*120))&&(MOD_VOL <(3855*120))){SOC=70;}
	else if((MOD_VOL >(3836*120))&&(MOD_VOL <(3845*120))){SOC=69;}
	else if((MOD_VOL >(3827*120))&&(MOD_VOL <(3836*120))){SOC=68;}
	else if((MOD_VOL >(3818*120))&&(MOD_VOL <(3827*120))){SOC=67;}
	else if((MOD_VOL >(3809*120))&&(MOD_VOL <(3818*120))){SOC=66;}
	else if((MOD_VOL >(3800*120))&&(MOD_VOL <(3809*120))){SOC=65;}
	else if((MOD_VOL >(3792*120))&&(MOD_VOL <(3800*120))){SOC=64;}
	else if((MOD_VOL >(3784*120))&&(MOD_VOL <(3792*120))){SOC=63;}
	else if((MOD_VOL >(3775*120))&&(MOD_VOL <(3784*120))){SOC=62;}
	else if((MOD_VOL >(3768*120))&&(MOD_VOL <(3775*120))){SOC=61;}
	else if((MOD_VOL >(3760*120))&&(MOD_VOL <(3768*120))){SOC=60;}
	else if((MOD_VOL >(3753*120))&&(MOD_VOL <(3768*120))){SOC=59;}
	else if((MOD_VOL >(3745*120))&&(MOD_VOL <(3753*120))){SOC=58;}
	else if((MOD_VOL >(3738*120))&&(MOD_VOL <(3745*120))){SOC=57;}
	else if((MOD_VOL >(3732*120))&&(MOD_VOL <(3738*120))){SOC=56;}
	else if((MOD_VOL >(3725*120))&&(MOD_VOL <(3732*120))){SOC=55;}
	else if((MOD_VOL >(3719*120))&&(MOD_VOL <(3725*120))){SOC=54;}
	else if((MOD_VOL >(3713*120))&&(MOD_VOL <(3719*120))){SOC=53;}
	else if((MOD_VOL >(3707*120))&&(MOD_VOL <(3713*120))){SOC=52;}
	else if((MOD_VOL >(3701*120))&&(MOD_VOL <(3707*120))){SOC=51;}
	else if((MOD_VOL >(3695*120))&&(MOD_VOL <(3701*120))){SOC=50;}
	else if((MOD_VOL >(3690*120))&&(MOD_VOL <(3695*120))){SOC=49;}
	else if((MOD_VOL >(3685*120))&&(MOD_VOL <(3690*120))){SOC=48;}
	else if((MOD_VOL >(3679*120))&&(MOD_VOL <(3685*120))){SOC=47;}
	else if((MOD_VOL >(3675*120))&&(MOD_VOL <(3679*120))){SOC=46;}
	else if((MOD_VOL >(3670*120))&&(MOD_VOL <(3675*120))){SOC=45;}
	else if((MOD_VOL >(3665*120))&&(MOD_VOL <(3670*120))){SOC=44;}
	else if((MOD_VOL >(3661*120))&&(MOD_VOL <(3665*120))){SOC=43;}
	else if((MOD_VOL >(3657*120))&&(MOD_VOL <(3661*120))){SOC=42;}
	else if((MOD_VOL >(3652*120))&&(MOD_VOL <(3657*120))){SOC=41;}
	else if((MOD_VOL >(3648*120))&&(MOD_VOL <(3652*120))){SOC=40;}
	else if((MOD_VOL >(3645*120))&&(MOD_VOL <(3648*120))){SOC=39;}
	else if((MOD_VOL >(3641*120))&&(MOD_VOL <(3645*120))){SOC=38;}
	else if((MOD_VOL >(3637*120))&&(MOD_VOL <(3641*120))){SOC=37;}
	else if((MOD_VOL >(3633*120))&&(MOD_VOL <(3637*120))){SOC=36;}
	else if((MOD_VOL >(3630*120))&&(MOD_VOL <(3633*120))){SOC=35;}
	else if((MOD_VOL >(3626*120))&&(MOD_VOL <(3630*120))){SOC=34;}
	else if((MOD_VOL >(3623*120))&&(MOD_VOL <(3626*120))){SOC=33;}
	else if((MOD_VOL >(3619*120))&&(MOD_VOL <(3623*120))){SOC=32;}
	else if((MOD_VOL >(3616*120))&&(MOD_VOL <(3619*120))){SOC=31;}
	else if((MOD_VOL >(3613*120))&&(MOD_VOL <(3616*120))){SOC=30;}
	else if((MOD_VOL >(3609*120))&&(MOD_VOL <(3613*120))){SOC=29;}
	else if((MOD_VOL >(3606*120))&&(MOD_VOL <(3609*120))){SOC=28;}
	else if((MOD_VOL >(3602*120))&&(MOD_VOL <(3606*120))){SOC=27;}
	else if((MOD_VOL >(3599*120))&&(MOD_VOL <(3602*120))){SOC=26;}
	else if((MOD_VOL >(3595*120))&&(MOD_VOL <(3599*120))){SOC=25;}
	else if((MOD_VOL >(3592*120))&&(MOD_VOL <(3595*120))){SOC=24;}
	else if((MOD_VOL >(3588*120))&&(MOD_VOL <(3292*120))){SOC=23;}
	else if((MOD_VOL >(3584*120))&&(MOD_VOL <(3588*120))){SOC=22;}
	else if((MOD_VOL >(3580*120))&&(MOD_VOL <(3584*120))){SOC=21;}
	else if((MOD_VOL >(3576*120))&&(MOD_VOL <(3580*120))){SOC=20;}
	else if((MOD_VOL >(3571*120))&&(MOD_VOL <(3576*120))){SOC=19;}
	else if((MOD_VOL >(3567*120))&&(MOD_VOL <(3571*120))){SOC=18;}
	else if((MOD_VOL >(3562*120))&&(MOD_VOL <(3567*120))){SOC=17;}
	else if((MOD_VOL >(3557*120))&&(MOD_VOL <(3562*120))){SOC=16;}
	else if((MOD_VOL >(3552*120))&&(MOD_VOL <(3557*120))){SOC=15;}
	else if((MOD_VOL >(3546*120))&&(MOD_VOL <(3552*120))){SOC=14;}
	else if((MOD_VOL >(3540*120))&&(MOD_VOL <(3546*120))){SOC=13;}
	else if((MOD_VOL >(3534*120))&&(MOD_VOL <(3540*120))){SOC=12;}
	else if((MOD_VOL >(3528*120))&&(MOD_VOL <(3534*120))){SOC=11;}
	else if((MOD_VOL >(3521*120))&&(MOD_VOL <(3528*120))){SOC=10;}
	else if((MOD_VOL >(3513*120))&&(MOD_VOL <(3521*120))){SOC=9;}
	else if((MOD_VOL >(3506*120))&&(MOD_VOL <(3513*120))){SOC=8;}
	else if((MOD_VOL >(3498*120))&&(MOD_VOL <(3506*120))){SOC=7;}
	else if((MOD_VOL >(3489*120))&&(MOD_VOL <(3498*120))){SOC=6;}
	else if((MOD_VOL >(3480*120))&&(MOD_VOL <(3489*120))){SOC=5;}
	else if((MOD_VOL >(3470*120))&&(MOD_VOL <(3480*120))){SOC=4;}
	else if((MOD_VOL >(3460*120))&&(MOD_VOL <(3470*120))){SOC=3;}
	else if((MOD_VOL >(3449*120))&&(MOD_VOL <(3460*120))){SOC=2;}
	else if((MOD_VOL >(3438*120))&&(MOD_VOL <(3449*120))){SOC=1;}
	else if( MOD_VOL <(3438*120)){SOC=0;}
    
    printf("SOC:%d \r\n",SOC);

    //osal_mdelay(10);

    return SOC;
}


float LTC6804_Calculate_Temperature(float adc_value) {
  float Rt =(100*adc_value)/(30000-adc_value);  //某温度下Rt的值
//	printf("Rt%f\r\n",Rt);
	float temp1=log(Rt/100);
	float temp2=temp1/4150;
	float temp3=1/298.15;
	float T=((1/(temp2+temp3))-273.15)*1000;  //计算温度
  return T;}


/******************************************************************   END   *****************************************************************/

extern network_adv_data_t g_network_status;

void *bms_salve_task(void)
{
    uapi_watchdog_disable();
    app_spi_init_pin();
    app_spi_master_init_config();

    //ADC
    uapi_adc_init(ADC_CLOCK_NONE);
    uint8_t adc_channel = CONFIG_ADC_CHANNEL;
    uint16_t voltage = 0;

    uapi_pin_set_mode(13, HAL_PIO_FUNC_GPIO);
    uapi_pin_set_mode(14, HAL_PIO_FUNC_GPIO);
 // 设置 GPIO 引脚方向为输出
    uapi_gpio_set_dir(13, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_dir(14, GPIO_DIRECTION_OUTPUT);
 // 初始化 GPIO 引脚为低电平（LED 关闭）
    uapi_gpio_set_val(13, GPIO_LEVEL_HIGH);
    uapi_gpio_set_val(14, GPIO_LEVEL_HIGH);

    LTC6804_initialize();//通讯正常
    printf("\r\n");
    osal_mdelay(10);
    int Current = 0.0f;
    

    while (1) {
        MOD_VOL = 0;
        for(int i=0;i<12;i++)
        {
            MOD_VOL += cell_codes[0][i];
        }
        Get_SOC();
        Get_Cell_Voltage_Max_Min();
	    Balance_task(300);
        wakeup_sleep();
        LTC6804_adcv();
        osal_mdelay(10);
        wakeup_idle();
        LTC6804_rdcv(0, Total_IC, cell_codes);
        osal_mdelay(10);
        wakeup_idle();
        LTC6804_adax();//05 e0
        osal_mdelay(10);
        LTC6804_rdaux(0, Total_IC, gpiocode);//00 0c    00 0e
        osal_mdelay(10);
        adc_port_read(adc_channel, &voltage);
         printf("ADC Voltage:%d \r\n",voltage);
        Current = (5.0/5.0*voltage/1000-2.5)/66.7*1000*10000;
        osal_printk("Current: %d\r\n", Current);

        for(int i = 0; i < 12; i++)//过压欠压告警
        {
            if((int)cell_codes[0][i] > 42000)
            {
                printf("OVER VOLTAGE! STOP Charging!\r\n");
                osal_mdelay(10);
            }
            if((int)cell_codes[0][i] < 32400)
            {
                printf("LOW VOLTAGE! Charge Please!\r\n");
                osal_mdelay(10);
            }
            if((int)cell_codes[0][i] > 45000)//电压异常断电
            {
                uapi_gpio_set_val(13, GPIO_LEVEL_LOW);
                uapi_gpio_set_val(14, GPIO_LEVEL_LOW);
                printf("Abnormal Voltage! Power has been cut off!\r\n");
                osal_mdelay(10);
            }
        }

        for(int i = 0; i < 5; i++)//高温告警断电
        {
            if((int)LTC6804_Calculate_Temperature((int)gpiocode[0][i]) > 40000)
            {
                uapi_gpio_set_val(13, GPIO_LEVEL_LOW);
                uapi_gpio_set_val(14, GPIO_LEVEL_LOW);
                printf("OVER Temperature! Power has been cut off!\r\n");
                osal_mdelay(10);
            }
        }

        if(Current < 0.2)
        {
            printf("Charging Current:%d mA\r\n", Current);
        }
        else if(Current > 0.2 && Current < 30)
        {
            printf("Discharging Current:%d mA\r\n", Current);
        }
        

        
        
        // printf("VREF2:%d \r\n",(int)gpiocode[0][5]);//从控供电正常
        // osal_mdelay(10);
        // printf("Read GPIO return ֵ%d \r\n",a);//温度回读正常
        // osal_mdelay(10);
        // printf("Read Voltage return ֵ%d \r\n",r);//电压回读正常
        // osal_mdelay(10);

        // 构建BMS JSON字符串，返回json字符串指针和长度，需外部释放
        // 构建 cell_voltages 数组
        cJSON *cell_voltages = cJSON_CreateArray();
        for (int i = 0; i < 12; i++) {
            cJSON_AddItemToArray(cell_voltages, cJSON_CreateNumber(cell_codes[0][i]));
        }
        // 构建 temperatures 数组
        cJSON *temperatures = cJSON_CreateArray();
        for (int i = 0; i < 5; i++) {
            cJSON_AddItemToArray(temperatures, cJSON_CreateNumber((int)LTC6804_Calculate_Temperature((int)gpiocode[0][i])));
        }
        // 构建根对象
        cJSON *root = cJSON_CreateObject();

        // 将本机MAC地址添加到JSON对象中
        sle_addr_t *local_addr = hybrid_get_local_addr();
        char mac_str[18]; // xx:xx:xx:xx:xx:xx\0
        (void)snprintf_s(mac_str, sizeof(mac_str), sizeof(mac_str) - 1, "%02x:%02x:%02x:%02x:%02x:%02x",
                 local_addr->addr[0], local_addr->addr[1], local_addr->addr[2],
                 local_addr->addr[3], local_addr->addr[4], local_addr->addr[5]);
        cJSON_AddStringToObject(root, "origin_mac", mac_str);

        cJSON_AddNumberToObject(root, "total", MOD_VOL);
        cJSON_AddItemToObject(root, "cell", cell_voltages);
        cJSON_AddItemToObject(root, "temperature", temperatures);
        cJSON_AddNumberToObject(root, "current", Current); 
        cJSON_AddNumberToObject(root, "SOC", Get_SOC()); // 300mV 压差均衡开关
        
        // 打印格式JSON
        char *json_str = cJSON_Print(root);
        printf("BMS_JSON:%s\n", json_str);
        
        if (hybrid_node_get_role() == NODE_ROLE_MEMBER && sle_hybrids_is_client_connected()) {
            osal_printk("Member node sending its own JSON data...\r\n");
            // 直接发送JSON字符串，长度+1以包含末尾的'\0'
            sle_hybrids_send_data((uint8_t*)json_str, strlen(json_str) + 1);
        }

        // 释放内存
        cJSON_Delete(root);
        free(json_str);
        
        osal_mdelay(1000); // 适当延长发送间隔
    }
    return 0;
}

static void bms_slave_entry(void)
{
    osal_task *task_handle = NULL;
    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)bms_salve_task, 0, "bms_salve_task", SPI_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, SPI_TASK_PRIO);
        osal_kfree(task_handle);
    }
    osal_kthread_unlock();
}

/* Run the bms_salve_entry. */
app_run(bms_slave_entry);