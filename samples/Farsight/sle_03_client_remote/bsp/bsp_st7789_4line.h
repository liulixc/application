/**
 ******************************************************************************
 * @file   bsp_st7789_4line.h
 * @brief  1.14寸屏ST7789驱动文件，采用4线SPI
 *
 ******************************************************************************
 */
#ifndef __BSP_ST7789_4LINE_H__
#define __BSP_ST7789_4LINE_H__
#include "spi.h"
#include "gpio.h"
#include "pinctrl.h"
#include "osal_debug.h"
#include "osal_task.h"
typedef struct {
    uint16_t width;  // ST7789 宽度
    uint16_t height; // ST7789 高度
    uint16_t id;     // ST7789 ID
    uint8_t wramcmd; // 开始写gram指令
    uint8_t setxcmd; // 设置x坐标指令
    uint8_t setycmd; // 设置y坐标指令
    uint8_t xoffset; // x坐标偏移
    uint8_t yoffset; // y坐标偏移
} _ST7789_dev;
// 扫描方向定义
#define L2R_U2D 0 // 从左到右,从上到下
#define L2R_D2U 1 // 从左到右,从下到上
#define R2L_U2D 2 // 从右到左,从上到下
#define R2L_D2U 3 // 从右到左,从下到上

#define U2D_L2R 4 // 从上到下,从左到右
#define U2D_R2L 5 // 从上到下,从右到左
#define D2U_L2R 6 // 从下到上,从左到右
#define D2U_R2L 7 // 从下到上,从右到左
// 屏幕显示方式
typedef enum {
    SCAN_Vertical = 0U, // 竖屏
    SCAN_Horizontal     // 横屏
} Screen_ShowDIR;

#define LCD_RST_GPIO_Port GPIOA
#define LCD_DCX_GPIO_Port GPIOA
#define LCD_CS_GPIO_Port GPIOA
#define LCD_DCX_Pin GPIO_PIN_4
#define LCD_CS_Pin GPIO_PIN_6
#define LCD_RST_Pin GPIO_PIN_8
// 画笔颜色
#define WHITE 0xFFFF
#define BLACK 0x0000
#define BLUE 0x001F
#define BRED 0XF81F
#define GRED 0XFFE0
#define GBLUE 0X07FF
#define RED 0xF800
#define MAGENTA 0xF81F
#define GREEN 0x07E0
#define CYAN 0x7FFF
#define YELLOW 0xFFE0
#define BROWN 0XBC40 // 棕色
#define BRRED 0XFC07 // 棕红色
#define GRAY 0X8430  // 灰色
// GUI颜色
#define DARKBLUE 0X01CF  // 深蓝色
#define LIGHTBLUE 0X7D7C // 浅蓝色
#define GRAYBLUE 0X5458  // 灰蓝色
// 以上三色为PANEL的颜色
#define LIGHTGREEN 0X841F // 浅绿色
// #define LIGHTGRAY        0XEF5B //浅灰色(PANNEL)
#define LGRAY 0XC618 // 浅灰色(PANNEL),窗体背景色

#define LGRAYBLUE 0XA651 // 浅灰蓝色(中间层颜色)
#define LBBLUE 0X2B12    // 浅棕蓝色(选择条目的反色)
extern double car_power;
extern uint8_t car_speed, car_conn;
extern char sle_recv[50];
extern uint8_t sle_recv_flag;
void ST7789_Init(void);                                                  // 初始化
void ST7789_Clear(uint16_t Color);                                       // 清屏
void ST7789_SetCursor(uint16_t Xpos, uint16_t Ypos);                     // 设置光标
void ST7789_SetArea(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1); // 设置显示区域

void ST7789_WR_REG(uint8_t);
void ST7789_WR_DATA(uint8_t);
void ST7789_WriteReg(uint8_t ST7789_Reg, uint8_t ST7789_RegValue);

void ST7789_WriteRAM_Prepare(void);
void ST7789_WriteRAM(uint16_t RGB_Code);
void ST7789_Display_Dir(Screen_ShowDIR ShowDIR); // 设置屏幕显示方向

void LCD_DrawRect(uint16_t _usX,
                  uint16_t _usY,
                  uint16_t _usHeight,
                  uint16_t _usWidth,
                  uint16_t _usColor);                                                     // 绘制水平放置的矩形
void LCD_DrawCircle(uint16_t _usX, uint16_t _usY, uint16_t _usRadius, uint16_t _usColor); // 绘制一个圆，笔宽为1个像素

void HW_DisplayImage(uint16_t xStart, uint16_t yStart, uint16_t width, uint16_t height, const uint8_t *image);
void _HW_DrawLine(uint16_t _usX1, uint16_t _usY1, uint16_t _usX2, uint16_t _usY2, uint16_t _usColor);
void LCD_DrawRect(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, uint16_t _usColor);
void LCD_ShowChar(uint16_t x, uint16_t y, char ch, uint16_t back_color, uint16_t font_color, uint8_t font_size);
void LCD_ShowCharStr(uint16_t x,
                     uint16_t y,
                     uint8_t max_width,
                     char *str,
                     uint16_t back_color,
                     uint16_t font_color,
                     uint8_t font_size);
void ST7789_ShowChar(uint16_t xStart, uint16_t yStart, char ch, const uint8_t *charData);
void TFT_display_char16_16(const uint8_t *address, uint16_t startX, uint16_t startY, uint16_t color);
void ST7789_ShowChineseChar(uint16_t xStart, uint16_t yStart, const uint8_t *chineseCharData, uint16_t color);
void DisplayChar(uint16_t xStart, uint16_t yStart, char c, uint16_t color);
void DisplayChineseCharacter(uint16_t x_start, uint16_t y_start, uint8_t char_index, uint16_t color);
void DisplayChineseString(uint16_t x_start, uint16_t y_start, uint8_t *str, uint16_t color);
void DisplayString_chat(uint16_t xStart, uint16_t yStart, uint16_t color, const char *str);

void app_spi_init_pin(void);
void app_spi_master_init_config(void);

void car_state_Parse(char *str);
void remote_display(void);
#endif /* __BSP_ST7789_4LINE_H__ */
