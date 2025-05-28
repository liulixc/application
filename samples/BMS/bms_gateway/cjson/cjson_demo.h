/*
 * Copyright (c) 2024 HiSilicon Technologies CO., Ltd.
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

#ifndef CJSON_DEMO_H
#define CJSON_DEMO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include "cJSON.h"

char *make_json(char *service_id, char *temperature, char *humidity);
char *parse_json(char *json_string);
char *combine_strings(int str_amount, char *str1, ...);

char *build_and_print_bms_json(uint16_t cell_codes[1][12], uint16_t gpiocode[1][6], int MOD_VOL, int *out_len);
#endif