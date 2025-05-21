/**
 ******************************************************************************
 * @file   bsp_st7789_4line.c
 * @brief  1.14寸屏ST7789驱动文件，采用4线SPI
 *
 ******************************************************************************
 */
#include "bsp_st7789_4line.h"
#include "spi.h"
#include "font.h"
#include "string.h"
extern uint8_t select_mode;
double car_power;
uint8_t car_speed, car_conn;
char sle_recv[50];
uint8_t sle_recv_flag;
// SPI
#define SPI_SLAVE_NUM 1
#define SPI_BUS_CLK 24000000
#define SPI_FREQUENCY 10
#define SPI_FRAME_FORMAT_STANDARD 0
#define SPI_WAIT_CYCLES 0x10
#define SPI_RST_MASTER_PIN 8
#define SPI_CS_MASTER_PIN 10
#define SPI_DO_MASTER_PIN 9
#define SPI_CLK_MASTER_PIN 7
#define SPI_DC_MASTER_PIN 11
#define SPI_MASTER_PIN_MODE 3
#define SPI_MASTER_BUS_ID 0

char oledShowBuff[30] = {0};
static uint8_t DFT_SCAN_DIR; // 扫描方向
// 管理ST7789重要参数
static _ST7789_dev ST7789dev;

void app_spi_init_pin(void)
{
    uapi_pin_set_mode(SPI_RST_MASTER_PIN, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(SPI_RST_MASTER_PIN, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(SPI_RST_MASTER_PIN, GPIO_LEVEL_HIGH);

    uapi_pin_set_mode(SPI_CS_MASTER_PIN, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(SPI_CS_MASTER_PIN, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(SPI_CS_MASTER_PIN, GPIO_LEVEL_LOW);

    uapi_pin_set_mode(SPI_DC_MASTER_PIN, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(SPI_DC_MASTER_PIN, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(SPI_DC_MASTER_PIN, GPIO_LEVEL_HIGH);

    uapi_pin_set_mode(SPI_DO_MASTER_PIN, SPI_MASTER_PIN_MODE);
    uapi_pin_set_mode(SPI_CLK_MASTER_PIN, SPI_MASTER_PIN_MODE);
}

void app_spi_master_init_config(void)
{
    spi_attr_t config = {0};
    spi_extra_attr_t ext_config = {0};

    config.is_slave = false;
    config.slave_num = SPI_SLAVE_NUM;
    config.bus_clk = SPI_BUS_CLK;
    config.freq_mhz = SPI_FREQUENCY;
    config.clk_polarity = SPI_CFG_CLK_CPOL_1;
    config.clk_phase = SPI_CFG_CLK_CPHA_1;
    config.frame_format = SPI_CFG_FRAME_FORMAT_MOTOROLA_SPI;
    config.spi_frame_format = HAL_SPI_FRAME_FORMAT_STANDARD;
    // 80001338代表发送数据的格式不对，需要修改配置参数
    config.frame_size = HAL_SPI_FRAME_SIZE_8;
    config.tmod = HAL_SPI_TRANS_MODE_TX;
    config.sste = 0;

    // ext_config.qspi_param.wait_cycles = SPI_WAIT_CYCLES;
    int ret = uapi_spi_init(SPI_MASTER_BUS_ID, &config, &ext_config);
    if (ret != 0) {
        printf("spi init fail %0x\r\n", ret);
    }
}

uint32_t HAL_SPI_Transmit(spi_bus_t bus, uint8_t *txdata, uint32_t data_len, uint32_t timeout)
{
    uint8_t *buffer = txdata;
    spi_xfer_data_t data = {0};

    data.tx_buff = buffer;
    data.tx_bytes = data_len;
    uapi_spi_master_write(bus, &data, timeout);
    return 0;
}
/*
**********************************************************************
* @fun     :ST7789_WR_REG
* @brief   :写寄存器函数
* @param   :REG:寄存器值
* @return  :None
**********************************************************************
*/
inline void ST7789_WR_REG(uint8_t REG)
{

    uapi_gpio_set_val(SPI_CS_MASTER_PIN, GPIO_LEVEL_LOW);
    // 写寄存器
    uapi_gpio_set_val(SPI_DC_MASTER_PIN, GPIO_LEVEL_LOW);

    HAL_SPI_Transmit(SPI_MASTER_BUS_ID, &REG, 1, 0xffffffff); // 不读取从机返回数据

    uapi_gpio_set_val(SPI_DC_MASTER_PIN, GPIO_LEVEL_HIGH);
}
/*
**********************************************************************
* @fun     :ST7789_WR_DATA
* @brief   :写ST7789数据
* @param   :DATA:要写入的值
* @return  :None
**********************************************************************
*/
inline void ST7789_WR_DATA(uint8_t DATA)
{
    // 写入数据
    HAL_SPI_Transmit(SPI_MASTER_BUS_ID, &DATA, 1, 0xffffffff); // 不读取从机返回数据
}
/*
**********************************************************************
* @fun     :ST7789_WriteReg
* @brief   :ST7789_Reg:寄存器地址，ST7789_RegValue:要写入的数据
* @param   :
* @return  :None
**********************************************************************
*/
inline void ST7789_WriteReg(uint8_t ST7789_Reg, uint8_t ST7789_RegValue)
{
    ST7789_WR_REG(ST7789_Reg);
    ST7789_WR_DATA(ST7789_RegValue);
}
/*
**********************************************************************
* @fun     :ST7789_WriteRAM_Prepare
* @brief   :开始写GRAM
* @param   :
* @return  :None
**********************************************************************
*/
inline void ST7789_WriteRAM_Prepare(void)
{
    ST7789_WR_REG(ST7789dev.wramcmd);
}
/*
**********************************************************************
* @fun     :ST7789_WriteRAM
* @brief   :ST7789写GRAM，SPI数据写入方式不同，功能同ST7789_WriteRAM_Prepare
* @param   :
* @return  :None
**********************************************************************
*/
inline void ST7789_WriteRAM(uint16_t DAT)
{
    uint8_t TempBufferD[2] = {DAT >> 8, DAT};

    // 写入数据
    HAL_SPI_Transmit(SPI_MASTER_BUS_ID, TempBufferD, 2, 0xffffffff); // 不读取从机返回数据
}
/*
**********************************************************************
* @fun     :ST7789_SetCursor
* @brief   :设置光标位置
* @param   :Xpos:横坐标，Ypos:纵坐标
* @return  :None
**********************************************************************
*/
void ST7789_SetCursor(uint16_t Xpos, uint16_t Ypos)
{
    uint8_t TempBufferX[2] = {(Xpos + ST7789dev.xoffset) >> 8, (Xpos + ST7789dev.xoffset) & 0XFF};
    uint8_t TempBufferY[2] = {(Ypos + ST7789dev.yoffset) >> 8, (Ypos + ST7789dev.yoffset) & 0XFF};
    // 设置坐标
    ST7789_WR_REG(ST7789dev.setxcmd);

    // 传输数据
    HAL_SPI_Transmit(SPI_MASTER_BUS_ID, TempBufferX, 2, 0xffffffff);

    // 设置坐标
    ST7789_WR_REG(ST7789dev.setycmd);

    // 传输数据
    HAL_SPI_Transmit(SPI_MASTER_BUS_ID, TempBufferY, 2, 0xffffffff);
}
/*
**********************************************************************
* @fun     :ST7789_SetArea
* @brief   :设置显示区域
* @param   :x0/x1:横坐标，y0/y1:纵坐标
* @return  :None
**********************************************************************
*/

void ST7789_SetArea(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t arguments[4];
    uint16_t old_x0 = 0xFFFF, old_x1 = 0xFFFF, old_y0 = 0xFFFF, old_y1 = 0xFFFF;
    // Set columns, if changed
    if (x0 != old_x0 || x1 != old_x1) {
        arguments[0] = x0 >> 8;
        arguments[1] = x0 & 0xFF;
        arguments[2] = x1 >> 8;
        arguments[3] = x1 & 0xFF;
        // 写寄存器
        ST7789_WR_REG(ST7789dev.setxcmd);

        // 传输数据
        HAL_SPI_Transmit(SPI_MASTER_BUS_ID, arguments, 4, 0xffffffff);

        //
        old_x0 = x0;
        old_x1 = x1;
    }
    // Set rows, if changed
    if (y0 != old_y0 || y1 != old_y1) {
        arguments[0] = y0 >> 8;
        arguments[1] = y0 & 0xFF;
        arguments[2] = y1 >> 8;
        arguments[3] = y1 & 0xFF;
        // 写寄存器
        ST7789_WR_REG(ST7789dev.setycmd);

        // 传输数据
        HAL_SPI_Transmit(SPI_MASTER_BUS_ID, arguments, 4, 0xffffffff);

        //
        old_y0 = y0;
        old_y1 = y1;
    }
}
/*
**********************************************************************
* @fun     :ST7789_Display_Dir
* @brief   :设置ST7789的自动扫描方向
                            Memory Access Control (36h)
                            This command defines read/write scanning direction of the frame memory.

                            These 3 bits control the direction from the MPU to memory write/read.

                            Bit  Symbol  Name  Description
                            D7   MY  Row Address Order
                            D6   MX  Column Address Order
                            D5   MV  Row/Column Exchange
                            D4   ML  Vertical Refresh Order  LCD vertical refresh direction control. 、

                            D3   BGR RGB-BGR Order   Color selector switch control
                                        (0 = RGB color filter panel, 1 = BGR color filter panel )
                            D2   MH  Horizontal Refresh ORDER  LCD horizontal refreshing direction control.
                            D1   X   Reserved  Reserved
                            D0   X   Reserved  Reserved
* @param   :
* @return  :None
**********************************************************************
*/
void ST7789_Display_Dir(Screen_ShowDIR ShowDIR)
{
    uint16_t regval = 0x00; // RGB Order不能改变
    uint8_t dirreg = 0;

    if (ShowDIR == SCAN_Vertical) // 竖屏
    {
        ST7789dev.width = 135;
        ST7789dev.height = 240;

        ST7789dev.wramcmd = 0X2C;
        ST7789dev.setxcmd = 0X2A;
        ST7789dev.setycmd = 0X2B;
        DFT_SCAN_DIR = R2L_D2U;

        switch (DFT_SCAN_DIR) {
            case L2R_U2D: // 从左到右,从上到下  //竖屏
                regval |= (0 << 7) | (0 << 6) | (0 << 5) | (0 << 4);
                ST7789dev.xoffset = 52;
                ST7789dev.yoffset = 40;
                break;
            case R2L_D2U: // 从右到左,从下到上   //竖屏
                regval |= (1 << 7) | (1 << 6) | (0 << 5) | (0 << 4);
                ST7789dev.xoffset = 53;
                ST7789dev.yoffset = 40;
                break;
        }
    } else // 横屏
    {
        ST7789dev.width = 240;
        ST7789dev.height = 135;

        ST7789dev.wramcmd = 0X2C;
        ST7789dev.setxcmd = 0X2A;
        ST7789dev.setycmd = 0X2B;
        DFT_SCAN_DIR = U2D_R2L;

        switch (DFT_SCAN_DIR) {
            case U2D_R2L: // 从上到下,从右到左  //横屏
                regval |= (0 << 7) | (1 << 6) | (1 << 5) | (1 << 4);
                ST7789dev.xoffset = 40;
                ST7789dev.yoffset = 53;
                break;
            case D2U_L2R: // 从下到上,从左到右  //横屏
                regval |= (1 << 7) | (0 << 6) | (1 << 5) | (0 << 4);
                ST7789dev.xoffset = 40;
                ST7789dev.yoffset = 52;
                break;
        }
    }
    dirreg = 0X36;
    regval |= 0x00;
    ST7789_WriteReg(dirreg, regval);
    // 设置光标在原点位置
    ST7789_WR_REG(ST7789dev.setxcmd);
    ST7789_WR_DATA(ST7789dev.xoffset >> 8);
    ST7789_WR_DATA(ST7789dev.xoffset & 0XFF);
    ST7789_WR_DATA((ST7789dev.xoffset + ST7789dev.width - 1) >> 8);
    ST7789_WR_DATA((ST7789dev.xoffset + ST7789dev.width - 1) & 0XFF);

    ST7789_WR_REG(ST7789dev.setycmd);
    ST7789_WR_DATA(ST7789dev.yoffset >> 8);
    ST7789_WR_DATA(ST7789dev.yoffset & 0XFF);
    ST7789_WR_DATA((ST7789dev.yoffset + ST7789dev.height - 1) >> 8);
    ST7789_WR_DATA((ST7789dev.yoffset + ST7789dev.height - 1) & 0XFF);
}
/*
**********************************************************************
* @fun     :ST7789_Init
* @brief   :初始化ST7789
* @param   :
* @return  :None
**********************************************************************
*/
void ST7789_Init(void)
{
    // ST7789复位
    uapi_gpio_set_val(SPI_RST_MASTER_PIN, 0);
    osal_msleep(100);
    uapi_gpio_set_val(SPI_RST_MASTER_PIN, 1);
    osal_msleep(100);
    // 关闭睡眠模式
    ST7789_WR_REG(0x11);
    osal_msleep(120);
    // 设置横竖屏
    ST7789_WR_REG(0x36);
    ST7789_WR_REG(0x00);
    // RGB 5-6-5-bit 格式
    ST7789_WR_REG(0x3A);
    ST7789_WR_DATA(0x05);
    // porch 设置
    ST7789_WR_REG(0xB2);
    ST7789_WR_DATA(0x0C);
    ST7789_WR_DATA(0x0C);
    ST7789_WR_DATA(0x00);
    ST7789_WR_DATA(0x33);
    ST7789_WR_DATA(0x33);
    // VGH 设置
    ST7789_WR_REG(0xB7);
    ST7789_WR_DATA(0x35);
    // VCOM 设置
    ST7789_WR_REG(0xBB);
    ST7789_WR_DATA(0x19);
    // LCM 设置
    ST7789_WR_REG(0xC0);
    ST7789_WR_DATA(0x2C);
    // VDV and VRH 设置
    ST7789_WR_REG(0xC2);
    ST7789_WR_DATA(0x01);
    // VRH 设置
    ST7789_WR_REG(0xC3);
    ST7789_WR_DATA(0x12);
    // VDV 设置
    ST7789_WR_REG(0xC4);
    ST7789_WR_DATA(0x20);
    // 普通模式下显存速率设置
    ST7789_WR_REG(0xC6);
    ST7789_WR_DATA(0x0F);
    // 电源控制
    ST7789_WR_REG(0xD0);
    ST7789_WR_DATA(0xA4);
    ST7789_WR_DATA(0xA1);
    // 电压设置
    ST7789_WR_REG(0xE0);
    ST7789_WR_DATA(0xD0);
    ST7789_WR_DATA(0x04);
    ST7789_WR_DATA(0x0D);
    ST7789_WR_DATA(0x11);
    ST7789_WR_DATA(0x13);
    ST7789_WR_DATA(0x2B);
    ST7789_WR_DATA(0x3F);
    ST7789_WR_DATA(0x54);
    ST7789_WR_DATA(0x4C);
    ST7789_WR_DATA(0x18);
    ST7789_WR_DATA(0x0D);
    ST7789_WR_DATA(0x0B);
    ST7789_WR_DATA(0x1F);
    ST7789_WR_DATA(0x23);
    // 电压设置
    ST7789_WR_REG(0xE1);
    ST7789_WR_DATA(0xD0);
    ST7789_WR_DATA(0x04);
    ST7789_WR_DATA(0x0C);
    ST7789_WR_DATA(0x11);
    ST7789_WR_DATA(0x13);
    ST7789_WR_DATA(0x2C);
    ST7789_WR_DATA(0x3F);
    ST7789_WR_DATA(0x44);
    ST7789_WR_DATA(0x51);
    ST7789_WR_DATA(0x2F);
    ST7789_WR_DATA(0x1F);
    ST7789_WR_DATA(0x1F);
    ST7789_WR_DATA(0x20);
    ST7789_WR_DATA(0x23);
    // 显示开
    ST7789_WR_REG(0x21);
    ST7789_WR_REG(0x29);

    ST7789_Display_Dir(1); // 横屏显示

    ST7789_Clear(BLACK);
}
/*
**********************************************************************
* @fun     :ST7789_Clear
* @brief   :清屏函数，color:要清屏的填充色
* @param   :
* @return  :None
**********************************************************************
*/
void ST7789_Clear(uint16_t color)
{
    uint8_t TempBufferD[2] = {color >> 8, color};

    uint32_t index = 0;
    uint32_t totalpoint = ST7789dev.width;
    totalpoint *= ST7789dev.height; // 得到总点数

    ST7789_SetCursor(0x00, 0x00); // 设置光标位置
    ST7789_WriteRAM_Prepare();    // 开始写入GRAM

    // 传输数据
    for (index = 0; index < totalpoint; index++) {
        HAL_SPI_Transmit(SPI_MASTER_BUS_ID, TempBufferD, 2, 0xffffffff);
    }
}
/*
**********************************************************************
* @fun     :_HW_DrawPoint
* @brief   :uGUI函数调用，画点函数
* @param   :x,y:坐标，color:此点的颜色
* @return  :None
**********************************************************************
*/
void _HW_DrawPoint(uint16_t x, uint16_t y, uint16_t color)
{
    uint8_t TempBufferX[2] = {(x + ST7789dev.xoffset) >> 8, (x + ST7789dev.xoffset) & 0XFF};
    uint8_t TempBufferY[2] = {(y + ST7789dev.yoffset) >> 8, (y + ST7789dev.yoffset) & 0XFF};
    uint8_t TempBufferD[2] = {color >> 8, color};
    // 写寄存器
    ST7789_WR_REG(ST7789dev.setxcmd);

    // 传输数据
    HAL_SPI_Transmit(SPI_MASTER_BUS_ID, TempBufferX, 2, 0xffffffff);

    // 写寄存器
    ST7789_WR_REG(ST7789dev.setycmd);

    // 传输数据
    HAL_SPI_Transmit(SPI_MASTER_BUS_ID, TempBufferY, 2, 0xffffffff);

    // 写寄存器
    ST7789_WR_REG(ST7789dev.wramcmd);

    // 传输数据
    HAL_SPI_Transmit(SPI_MASTER_BUS_ID, TempBufferD, 2, 0xffffffff);
}
/*
**********************************************************************
* @fun     :_HW_FillFrame
* @brief   :在指定区域内填充单个颜色
* @param   :(sx,sy),(ex,ey):填充矩形对角坐标,区域大小为:(ex-sx+1)*(ey-sy+1),color:要填充的颜色
* @return  :None
**********************************************************************
*/
void _HW_FillFrame(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color)
{
    uint16_t i = 0, j = 0;
    uint16_t xlen = 0;
    xlen = ex - sx + 1;
    //
    uint8_t TempBuffer[2] = {color >> 8, color};

    // 数据写入屏幕
    for (i = sy; i <= ey; i++) {
        ST7789_SetCursor(sx, i);   // 设置光标位置
        ST7789_WriteRAM_Prepare(); // 开始写入GRAM
        for (j = 0; j < xlen; j++) {
            HAL_SPI_Transmit(SPI_MASTER_BUS_ID, TempBuffer, 2, 0xffffffff); // 点设置颜色
        }
    }
}
/*
**********************************************************************
* @fun     :_HW_DrawLine
* @brief   :画线
* @param   :_usX1,_usY1:起点坐标,_usX2,_usY2:终点坐标,_usColor:要填充的颜色
* @return  :None
**********************************************************************
*/
void _HW_DrawLine(uint16_t _usX1, uint16_t _usY1, uint16_t _usX2, uint16_t _usY2, uint16_t _usColor)
{
    int32_t dx, dy;
    int32_t tx, ty;
    int32_t inc1, inc2;
    int32_t d, iTag;
    int32_t x, y;
    /* 采用 Bresenham 算法，在2点间画一条直线 */
    _HW_DrawPoint(_usX1, _usY1, _usColor);
    /* 如果两点重合，结束后面的动作。*/
    if (_usX1 == _usX2 && _usY1 == _usY2) {
        return;
    }

    iTag = 0;
    /* dx = abs ( _usX2 - _usX1 ); */
    if (_usX2 >= _usX1) {
        dx = _usX2 - _usX1;
    } else {
        dx = _usX1 - _usX2;
    }

    /* dy =abs (_usY2-_usY1); */
    if (_usY2 >= _usY1) {
        dy = _usY2 - _usY1;
    } else {
        dy = _usY1 - _usY2;
    }

    if (dx < dy) /*如果dy为计长方向，则交换纵横坐标。*/
    {
        uint16_t temp;
        iTag = 1;
        temp = _usX1;
        _usX1 = _usY1;
        _usY1 = temp;
        temp = _usX2;
        _usX2 = _usY2;
        _usY2 = temp;
        temp = dx;
        dx = dy;
        dy = temp;
    }

    tx = _usX2 > _usX1 ? 1 : -1; /* 确定是增1还是减1 */
    ty = _usY2 > _usY1 ? 1 : -1;
    x = _usX1;
    y = _usY1;
    inc1 = 2 * dy;
    inc2 = 2 * (dy - dx);
    d = inc1 - dx;
    while (x != _usX2) /* 循环画点 */
    {
        if (d < 0) {
            d += inc1;
        } else {
            y += ty;
            d += inc2;
        }

        if (iTag) {
            _HW_DrawPoint(y, x, _usColor);
        } else {
            _HW_DrawPoint(x, y, _usColor);
        }

        x += tx;
    }
}
/*
**********************************************************************
* @fun     :LCD_DrawRect
* @brief   :绘制水平放置的矩形
* @param   :
*			_usX,_usY: 矩形左上角的坐标
*			_usHeight : 矩形的高度
*			_usWidth  : 矩形的宽度
* @return  :None
**********************************************************************
*/
void LCD_DrawRect(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, uint16_t _usColor)
{
    _HW_DrawLine(_usX, _usY, _usX + _usWidth - 1, _usY, _usColor);                                 /* 顶 */
    _HW_DrawLine(_usX, _usY + _usHeight - 1, _usX + _usWidth - 1, _usY + _usHeight - 1, _usColor); /* 底 */

    _HW_DrawLine(_usX, _usY, _usX, _usY + _usHeight - 1, _usColor);                           /* 左 */
    _HW_DrawLine(_usX + _usWidth - 1, _usY, _usX + _usWidth - 1, _usY + _usHeight, _usColor); /* 右 */
}
/*
**********************************************************************
* @fun     :LCD_DrawCircle
* @brief   :绘制一个圆，笔宽为1个像素
* @param   :
*			_usX,_usY  : 圆心的坐标
*			_usRadius  : 圆的半径
* @return  :None
**********************************************************************
*/
void LCD_DrawCircle(uint16_t _usX, uint16_t _usY, uint16_t _usRadius, uint16_t _usColor)
{
    int32_t D;     /* Decision Variable */
    uint32_t CurX; /* 当前 X 值 */
    uint32_t CurY; /* 当前 Y 值 */

    D = 3 - (_usRadius << 1);
    CurX = 0;
    CurY = _usRadius;

    while (CurX <= CurY) {
        _HW_DrawPoint(_usX + CurX, _usY + CurY, _usColor);
        _HW_DrawPoint(_usX + CurX, _usY - CurY, _usColor);
        _HW_DrawPoint(_usX - CurX, _usY + CurY, _usColor);
        _HW_DrawPoint(_usX - CurX, _usY - CurY, _usColor);
        _HW_DrawPoint(_usX + CurY, _usY + CurX, _usColor);
        _HW_DrawPoint(_usX + CurY, _usY - CurX, _usColor);
        _HW_DrawPoint(_usX - CurY, _usY + CurX, _usColor);
        _HW_DrawPoint(_usX - CurY, _usY - CurX, _usColor);

        if (D < 0) {
            D += (CurX << 2) + 6;
        } else {
            D += ((CurX - CurY) << 2) + 10;
            CurY--;
        }
        CurX++;
    }
}

void HW_DisplayImage(uint16_t xStart, uint16_t yStart, uint16_t width, uint16_t height, const uint8_t *image)
{
    uint16_t x, y;
    if (ST7789dev.width == 240)
        yStart += 50;
    for (y = 0; y < height; y++) {
        // 设置Y坐标
        uint8_t TempBufferY[2] = {(yStart + y) >> 8, (yStart + y) & 0xFF};
        // 写寄存器
        ST7789_WR_REG(ST7789dev.setycmd);

        // 传输数据
        HAL_SPI_Transmit(SPI_MASTER_BUS_ID, TempBufferY, 2, 0xffffffff);

        // 设置X坐标
        uint8_t TempBufferX[2] = {(xStart + 0) >> 8, (xStart + 0) & 0xFF};
        // 写寄存器
        ST7789_WR_REG(ST7789dev.setxcmd);

        // 传输数据
        HAL_SPI_Transmit(SPI_MASTER_BUS_ID, TempBufferX, 2, 0xffffffff);

        // 写寄存器
        ST7789_WR_REG(ST7789dev.wramcmd);

        for (x = 0; x < width; x++) {
            // 计算图片数据中的偏移量（每个像素2字节）
            uint32_t offset = (y * width * 2) + (x * 2);
            // 从image数据中读取高字节和低字节
            uint8_t highByte = image[offset];
            uint8_t lowByte = image[offset + 1];
            // 组合成RGB565格式的颜色值
            uint8_t TempBufferD[2] = {highByte, lowByte};
            // 传输颜色数据
            HAL_SPI_Transmit(SPI_MASTER_BUS_ID, TempBufferD, 2, 0xffffffff);
        }
    }
}

// 显示中文的函数
void DisplayChineseCharacter(uint16_t x_start, uint16_t y_start, uint8_t char_index, uint16_t color)
{
    const uint8_t *char_data = font_data_example[char_index];
    for (uint8_t y = 0; y < 32; y++) {
        for (uint8_t x = 0; x < 24 / 8; x++) { // 假设每个字节表示8个像素点
            uint8_t byte = char_data[y * (24 / 8) + x];
            for (uint8_t bit = 0; bit < 8; bit++) {
                uint8_t pixel = (byte >> (7 - bit)) & 0x01; // 从高位到低位检查每个位
                if (pixel) {
                    _HW_DrawPoint(x_start + x * 8 + bit, y_start + y, color);
                }
            }
        }
    }
}
// 显示箭头的函数
void DisplayChineseCharacter_icon(uint16_t x_start, uint16_t y_start, uint8_t char_index, uint16_t color)
{
    const uint8_t *char_data = font_data_example[char_index];
    for (uint8_t y = 0; y < 32; y++) {
        for (uint8_t x = 0; x < 32 / 8; x++) { // 假设每个字节表示8个像素点
            uint8_t byte = char_data[y * (32 / 8) + x];
            for (uint8_t bit = 0; bit < 8; bit++) {
                uint8_t pixel = (byte >> (7 - bit)) & 0x01; // 从高位到低位检查每个位
                if (pixel) {
                    _HW_DrawPoint(x_start + x * 8 + bit, y_start + y, color);
                }
            }
        }
    }
}

// 显示多个中文的函数
void DisplayChineseString(uint16_t x_start, uint16_t y_start, uint8_t *str, uint16_t color)
{
    uint16_t current_x = x_start; // 当前字符的起始x坐标
    uint16_t current_y = y_start; // 当前字符的起始y坐标

    while (*str != 99) {           // 遍历字符串直到遇到字符串结束符'\0'
        uint8_t char_index = *str; // 获取当前字符的索引，这里假设字符编码直接对应font_data_example的索引
        DisplayChineseCharacter(current_x, current_y, char_index, color); // 显示单个字符

        // 更新x坐标以便于下一个字符显示，每个字符宽度为32像素，间距为4像素
        current_x += 24 + 4;

        // 如果当前x位置超出了显示范围，则换行（这里简单处理，不考虑换行后的y位置溢出）
        if (current_x > (x_start + 400 - 24 - 4)) {
            current_x = x_start; // 重置x坐标
            current_y += 32;     // 更新y坐标，为下一行做准备
        }

        // 移动到字符串的下一个字符
        str++;
    }
}

// 显示字符的函数
void DisplayChineseChar(uint16_t x_start, uint16_t y_start, uint8_t char_index, uint16_t color, uint16_t bgcolor)
{
    const uint8_t *char_data = font16x32[char_index];
    uint16_t y;

    // 然后，遍历字符的每一行，绘制字符
    for (y = 0; y < 32; y++) {
        for (uint8_t byte_index = 0; byte_index < 1; byte_index++) { // 每行两个字节
            uint8_t byte = char_data[y * 1 + byte_index];            // 访问当前行的当前字节
            for (uint8_t bit = 0; bit < 8; bit++) {                  // 每个字节包含8个像素
                _HW_DrawPoint(x_start + bit, y_start + y, bgcolor);
                uint8_t pixel = (byte >> (7 - bit)) & 0x01; // 从高位到低位检查每个位
                if (pixel) {
                    // 注意修正x坐标的计算，确保正确绘制
                    _HW_DrawPoint(x_start + byte_index + bit, y_start + y, color);
                }
            }
        }
    }
}
// 显示字符串
void DisplayString_chat(uint16_t xStart, uint16_t yStart, uint16_t color, const char *str)
{
    uint16_t x_offset = 0; // 用于计算每个字符的X偏移量

    // 遍历字符串中的每个字
    while (*str) {
        uint8_t char_index = *str - ' '; // 假设字符从空格开始连续存储
        DisplayChineseChar(xStart + x_offset, yStart, char_index, color, BLACK); // 显示字符

        // 更新偏移量
        x_offset += 8; // 假设每个字符宽24像素，可以根据实际字符宽度调整
        str++;         // 移动到下一个字符
    }
}

void remote_display(void)
{

    if (sprintf(oledShowBuff, "ConTrol Mode: %s", ((select_mode > 0) ? " IR" : "SLE")) > 0) {
        DisplayString_chat(0, 0, LGRAY, oledShowBuff);
        memset_s(oledShowBuff, sizeof(oledShowBuff), 0, sizeof(oledShowBuff));
    }
    if (select_mode) {
        HW_DisplayImage(230, 0, 50, 50, gImage_ir);
    } else {
        HW_DisplayImage(230, 0, 50, 50, gImage_sle);
    }
    if (sprintf(oledShowBuff, "SLE Conn: %s", ((car_conn > 0) ? " OK" : "OFF")) > 0) {
        DisplayString_chat(0, 32, LGRAY, oledShowBuff);
        memset_s(oledShowBuff, sizeof(oledShowBuff), 0, sizeof(oledShowBuff));
    }
    if (sprintf(oledShowBuff, "Car Power: %.1f V", car_power) > 0) {
        DisplayString_chat(0, 64, LGRAY, oledShowBuff);
        memset_s(oledShowBuff, sizeof(oledShowBuff), 0, sizeof(oledShowBuff));
    }

    /* 显示小车当前的速度 */
    memset_s((char *)oledShowBuff, sizeof(oledShowBuff), 0, sizeof(oledShowBuff));
    if (car_speed == 0) {
        sprintf((char *)oledShowBuff, "speed: low   ");
    } else if (car_speed == 1) {
        sprintf((char *)oledShowBuff, "speed: middle");
    } else if (car_speed == 2) {
        sprintf((char *)oledShowBuff, "speed: high  ");
    }
    DisplayString_chat(0, 96, LGRAY, oledShowBuff);
    memset_s(oledShowBuff, sizeof(oledShowBuff), 0, sizeof(oledShowBuff));
}

void car_state_Parse(char *str)
{
    char *token = str + 1; // 跳过开头的 '{'

    // 解析 CAR_POWER
    token = strchr(token, ':') + 1;
    car_power = atof(token);
    token = strchr(token, ',');

    // 解析 CAR_SPEED
    token++;
    token = strchr(token, ':') + 1;
    car_speed = atoi(token);
    token = strchr(token, ',');

    // 解析 CAR_CONN
    token++;
    token = strchr(token, ':') + 1;
    car_conn = atoi(token);
}