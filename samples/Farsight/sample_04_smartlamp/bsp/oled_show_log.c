/*
 * Copyright (c) 2023 Beijing HuaQing YuanJian Education Technology Co., Ltd
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "hal_bsp_ssd1306.h"
#include "oled_show_log.h"

#define INIT_LINE 1 // 记录初始行数
#define MAX_LINE 4  // 记录最大行数
#define MAX_ROW 16  // 最大显示的字符个数为16个
typedef enum {
    LINE_1 = 1,
    LINE_2,
    LINE_3,
    LINE_4,
    LINE_5,
} te_Line_t;

/**
 * @brief  打印一行数据
 * @note   字体默认选择8*16,
 * @param  line: 行数, 最大行数为4行
 * @param  *string: 显示的字符串，最大显示的字符个数为16个
 * @retval None
 */
uint8_t oled_show_line_string(uint8_t line, char *string)
{
    if (line > MAX_LINE) {
        printf("line is > 4\r\n");
        return false;
    } else if (line <= 0) {
        printf("line is <= 0\r\n");
        return false;
    } else if (string == NULL) {
        printf("string is NULL\r\n");
        return false;
    } else if (strlen(string) > MAX_ROW) {
        printf("string of length is > 16\r\n");
        return false;
    }

    SSD1306_ShowStr(0, line - 1, string, TEXT_SIZE_16);
    return true;
}

static uint8_t current_line = INIT_LINE;
uint8_t oled_consle_log(char *string)
{
    if (string == NULL) {
        printf("string is NULL\r\n");
        return 1;
    }
    if (current_line > MAX_LINE) {
        current_line = INIT_LINE;
    }
    oled_show_line_string(current_line, "               "); // 清除这一行数据
    // 先打印首行
    if (current_line == INIT_LINE) {
        oled_show_line_string(current_line, string);
        // 打印首行的时候，将下面的两行进行删除
        oled_show_line_string(LINE_2, "               "); // 清除这一行数据
        oled_show_line_string(LINE_3, "               "); // 清除这一行数据
        oled_show_line_string(LINE_4, "               "); // 清除这一行数据
    } else {

        oled_show_line_string(current_line, string);
    }
    current_line++;
    return 0;
}
