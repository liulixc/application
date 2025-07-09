#include"cjson_demo.h"

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
        return NULL; // �����һ���ַ���Ϊ��
    }

    char *result = malloc(length);
    if (result == NULL) {
        return NULL; // �ڴ����ʧ��
    }

    strcpy(result, str1); // ���Ƶ�һ���ַ���

    va_list args;
    va_start(args, str1);

    char *tem_str;
    while (--str_amount > 0) {
        tem_str = va_arg(args, char *);
        if (tem_str == NULL) {
            continue; // �������ַ���
        }
        length += string_length(tem_str);
        result = realloc(result, length);
        if (result == NULL) {
            return NULL; // �ڴ����·���ʧ��
        }
        strcat(result, tem_str); // ƴ���ַ���
    }
    va_end(args);

    return result; // ����ƴ�Ӻ���ַ���
}

char *make_json(char *service_id, char *temperature, char *current)
{
    // ���� JSON ����
    cJSON *root = cJSON_CreateObject();
    // ���� services ����
    cJSON *services = cJSON_CreateArray();
    // ���� service ����
    cJSON *service = cJSON_CreateObject();
    cJSON_AddStringToObject(service, "service_id", service_id);

    // ���� properties ����
    cJSON *properties = cJSON_CreateObject();
    cJSON_AddStringToObject(properties, "temperature", temperature);
    cJSON_AddStringToObject(properties, "current", current);

    // �� properties ��ӵ� service
    cJSON_AddItemToObject(service, "properties", properties);

    // �� service ��ӵ� services ����
    cJSON_AddItemToArray(services, service);

    // �� services ��ӵ� root
    cJSON_AddItemToObject(root, "services", services);

    // ��ӡ JSON �ַ���
    char *json_string = cJSON_Print(root);
    // �ͷ��ڴ�
    cJSON_Delete(root);
    return json_string;
}

char *parse_json(char *json_string)
{
    char *string = NULL;
    // ���� JSON �ַ���
    cJSON *root = cJSON_Parse(json_string);
    if (root == NULL) {
        printf("Error parsing JSON\n");
        return NULL;
    }
    // ��ȡ paras �����е� beep ��
    cJSON *paras = cJSON_GetObjectItem(root, "paras");
    cJSON *beep = cJSON_GetObjectItem(paras, "beep");
    // ��鲢��� beep ��ֵ
    if (beep && cJSON_IsString(beep)) {
        printf("beep: %s\n", beep->valuestring);
        string = beep->valuestring;
    } else {
        printf("beep not found or is not a string\n");
    }
    // �ͷ��ڴ�
    cJSON_Delete(root);
    return string;
}