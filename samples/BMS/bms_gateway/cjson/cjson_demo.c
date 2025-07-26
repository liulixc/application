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
        return NULL; // Èç¹ûµÚÒ»¸ö×Ö·û´®Îª¿Õ
    }

    char *result = malloc(length);
    if (result == NULL) {
        return NULL; // ÄÚ´æ·ÖÅäÊ§°Ü
    }

    strcpy(result, str1); // ¸´ÖÆµÚÒ»¸ö×Ö·û´®

    va_list args;
    va_start(args, str1);

    char *tem_str;
    while (--str_amount > 0) {
        tem_str = va_arg(args, char *);
        if (tem_str == NULL) {
            continue; // Ìø¹ý¿Õ×Ö·û´®
        }
        length += string_length(tem_str);
        result = realloc(result, length);
        if (result == NULL) {
            return NULL; // ÄÚ´æÖØÐÂ·ÖÅäÊ§°Ü
        }
        strcat(result, tem_str); // Æ´½Ó×Ö·û´®
    }
    va_end(args);

    return result; // ·µ»ØÆ´½ÓºóµÄ×Ö·û´®
}

char *make_json(char *service_id, char *temperature, char *current)
{
    // ´´½¨ JSON ¶ÔÏó
    cJSON *root = cJSON_CreateObject();
    // ´´½¨ services Êý×é
    cJSON *services = cJSON_CreateArray();
    // ´´½¨ service ¶ÔÏó
    cJSON *service = cJSON_CreateObject();
    cJSON_AddStringToObject(service, "service_id", service_id);

    // ´´½¨ properties ¶ÔÏó
    cJSON *properties = cJSON_CreateObject();
    cJSON_AddStringToObject(properties, "temperature", temperature);
    cJSON_AddStringToObject(properties, "current", current);

    // ½« properties Ìí¼Óµ½ service
    cJSON_AddItemToObject(service, "properties", properties);

    // ½« service Ìí¼Óµ½ services Êý×é
    cJSON_AddItemToArray(services, service);

    // ½« services Ìí¼Óµ½ root
    cJSON_AddItemToObject(root, "services", services);

    // ´òÓ¡ JSON ×Ö·û´®
    char *json_string = cJSON_Print(root);
    // ÊÍ·ÅÄÚ´æ
    cJSON_Delete(root);
    return json_string;
}

char *parse_json(char *json_string)
{
    char *string = NULL;
    // ½âÎö JSON ×Ö·û´®
    cJSON *root = cJSON_Parse(json_string);
    if (root == NULL) {
        printf("Error parsing JSON\n");
        return NULL;
    }
    // »ñÈ¡ paras ¶ÔÏóÖÐµÄ beep Ïî
    cJSON *paras = cJSON_GetObjectItem(root, "paras");
    cJSON *beep = cJSON_GetObjectItem(paras, "beep");
    // ¼ì²é²¢Êä³ö beep µÄÖµ
    if (beep && cJSON_IsString(beep)) {
        printf("beep: %s\n", beep->valuestring);
        string = beep->valuestring;
    } else {
        printf("beep not found or is not a string\n");
    }
    // ÊÍ·ÅÄÚ´æ
    cJSON_Delete(root);
    return string;
}