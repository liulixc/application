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

#ifndef CJSON_UTILS_H
#define CJSON_UTILS_H

#include "cJSON.h"

char *make_json(char *service_id, char *temperature, char *humidity);
char *parse_json(char *json_string);
char *combine_strings(int str_amount, char *str1, ...);

// ͨ�ü�ֵ�Զ��󴴽�
cJSON *json_create_kv(int count, ...);
// ֧��Ƕ�׶���Ĵ���
cJSON *json_create_kv_nested(int count, ...);
// ͨ�����鴴��
cJSON *json_create_array(int count, ...);
// ��ʽ����ӡcJSON����
void json_print_pretty(const cJSON *json);
// ���մ�ӡcJSON����
void json_print_compact(const cJSON *json);

#endif