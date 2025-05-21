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

#ifndef OLED_SHOW_H
#define OLED_SHOW_H

#include "cmsis_os2.h"

#define OLED_DISPLAY_BUFF_SIZE 30   // OLED显示屏显示缓冲区最大为30

/**
 * @brief  打印一行数据
 * @note   字体默认选择8*16
 * @param  line: 行数, 最大行数为4行
 * @param  *string: 显示的字符串，最大显示的字符个数为16个
 * @retval None
 */
uint8_t oled_show_line_string(uint8_t line, char *string);

/**
 * @brief  按照终端的形式，一行行的打印输出
 * @note   默认显示行数为第2行
 * @param  *string: 显示的字符串，最大显示的字符个数为16个
 * @retval
 */
uint8_t oled_consle_log(char *string);

#endif
