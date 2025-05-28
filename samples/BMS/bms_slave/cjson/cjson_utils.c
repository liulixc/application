#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "cJSON.h"

int string_length(char *str)
{
    if (str == NULL) {
        return 0;
    }
    int len = 0;
    char *temp_str = str;
    while (*temp_str++ != '\0') {
        len++;
    }
    return len;
}

char *combine_strings(int str_amount, char *str1, ...)
{
    int length = string_length(str1) + 1;
    if (length == 1) {
        return NULL; // 如果第一个字符串为空
    }

    char *result = malloc(length);
    if (result == NULL) {
        return NULL; // 内存分配失败
    }

    strcpy(result, str1); // 复制第一个字符串

    va_list args;
    va_start(args, str1);

    char *tem_str;
    while (--str_amount > 0) {
        tem_str = va_arg(args, char *);
        if (tem_str == NULL) {
            continue; // 跳过空字符串
        }
        length += string_length(tem_str);
        result = realloc(result, length);
        if (result == NULL) {
            return NULL; // 内存重新分配失败
        }
        strcat(result, tem_str); // 拼接字符串
    }
    va_end(args);

    return result; // 返回拼接后的字符串
}

char *make_json(char *service_id, char *temperature, char *current)
{
    // 创建 JSON 对象
    cJSON *root = cJSON_CreateObject();
    // 创建 services 数组
    cJSON *services = cJSON_CreateArray();
    // 创建 service 对象
    cJSON *service = cJSON_CreateObject();
    cJSON_AddStringToObject(service, "service_id", service_id);

    // 创建 properties 对象
    cJSON *properties = cJSON_CreateObject();
    cJSON_AddStringToObject(properties, "temperature", temperature);
    cJSON_AddStringToObject(properties, "current", current);

    // 将 properties 添加到 service
    cJSON_AddItemToObject(service, "properties", properties);

    // 将 service 添加到 services 数组
    cJSON_AddItemToArray(services, service);

    // 将 services 添加到 root
    cJSON_AddItemToObject(root, "services", services);

    // 打印 JSON 字符串
    char *json_string = cJSON_Print(root);
    // 释放内存
    cJSON_Delete(root);
    return json_string;
}

char *parse_json(char *json_string)
{
    char *string = NULL;
    // 解析 JSON 字符串
    cJSON *root = cJSON_Parse(json_string);
    if (root == NULL) {
        printf("Error parsing JSON\n");
        return NULL;
    }
    // 获取 paras 对象中的 beep 项
    cJSON *paras = cJSON_GetObjectItem(root, "paras");
    cJSON *beep = cJSON_GetObjectItem(paras, "beep");
    // 检查并输出 beep 的值
    if (beep && cJSON_IsString(beep)) {
        printf("beep: %s\n", beep->valuestring);
        string = beep->valuestring;
    } else {
        printf("beep not found or is not a string\n");
    }
    // 释放内存
    cJSON_Delete(root);
    return string;
}

/*
 * 通用键值对JSON对象创建函数
 * count: 键值对数量
 * ...: 以(key, type, value)为一组，type=0表示字符串，type=1表示int
 * 返回cJSON对象，需外部cJSON_Delete释放
 * 示例：json_create_kv(3, "name", 0, "ws63", "age", 1, 18, "city", 0, "shanghai");
 */
cJSON *json_create_kv(int count, ...)
{
    cJSON *root = cJSON_CreateObject();
    va_list args;
    va_start(args, count);
    for (int i = 0; i < count; i++) {
        const char *key = va_arg(args, const char *);
        int type = va_arg(args, int);
        if (type == 0) { // 字符串
            const char *val = va_arg(args, const char *);
            cJSON_AddStringToObject(root, key, val);
        } else if (type == 1) { // int
            int val = va_arg(args, int);
            cJSON_AddNumberToObject(root, key, val);
        }
    }
    va_end(args);
    return root;
}

/*
 * 通用嵌套JSON对象创建函数
 * 支持一级嵌套：如{"name":"ws63", "age":18, "address":{"city":"shanghai", "street":"Nanjing Rd"}}
 *
 * 参数说明：
 *  count: 键值对数量
 *  ...: 以(key, type, value)为一组，type=0表示字符串，type=1表示int，type=2表示cJSON*（嵌套对象）
 *  嵌套对象需先用json_create_kv等函数创建好
 *
 * 示例：
 *  cJSON *addr = json_create_kv(2, "city", 0, "shanghai", "street", 0, "Nanjing Rd");
 *  cJSON *root = json_create_kv_nested(3, "name", 0, "ws63", "age", 1, 18, "address", 2, addr);
 */
cJSON *json_create_kv_nested(int count, ...)
{
    cJSON *root = cJSON_CreateObject();
    va_list args;
    va_start(args, count);
    for (int i = 0; i < count; i++) {
        const char *key = va_arg(args, const char *);
        int type = va_arg(args, int);
        if (type == 0) { // 字符串
            const char *val = va_arg(args, const char *);
            cJSON_AddStringToObject(root, key, val);
        } else if (type == 1) { // int
            int val = va_arg(args, int);
            cJSON_AddNumberToObject(root, key, val);
        } else if (type == 2) { // 嵌套cJSON对象
            cJSON *val = va_arg(args, cJSON *);
            cJSON_AddItemToObject(root, key, val);
        }
    }
    va_end(args);
    return root;
}

/*
 * 创建通用JSON数组对象
 * count: 元素数量
 * ...: 以(type, value)为一组，type=0表示字符串，type=1表示int，type=2表示cJSON*（嵌套对象/对象/数组）
 * 返回cJSON数组对象，需外部cJSON_Delete释放
 * 示例：
 *   cJSON *arr = json_create_array(3, 0, "ws63", 1, 18, 2, obj);
 */
cJSON *json_create_array(int count, ...)
{
    cJSON *array = cJSON_CreateArray();
    va_list args;
    va_start(args, count);
    for (int i = 0; i < count; i++) {
        int type = va_arg(args, int);
        if (type == 0) { // 字符串
            const char *val = va_arg(args, const char *);
            cJSON_AddItemToArray(array, cJSON_CreateString(val));
        } else if (type == 1) { // int
            int val = va_arg(args, int);
            cJSON_AddItemToArray(array, cJSON_CreateNumber(val));
        } else if (type == 2) { // 嵌套cJSON对象/数组
            cJSON *val = va_arg(args, cJSON *);
            cJSON_AddItemToArray(array, val);
        }
    }
    va_end(args);
    return array;
}

/*
 * 格式化打印cJSON对象
 */
void json_print_pretty(const cJSON *json)
{
    if (!json) {
        printf("json is NULL\n");
        return;
    }
    char *out = cJSON_Print(json);
    if (out) {
        printf("%s\n", out);
        free(out);
    }
}

/*
 * 紧凑格式打印cJSON对象（无缩进无换行）
 */
void json_print_compact(const cJSON *json)
{
    if (!json) {
        printf("json is NULL\n");
        return;
    }
    char *out = cJSON_PrintUnformatted(json);
    if (out) {
        printf("%s\n", out);
        free(out);
    }
}