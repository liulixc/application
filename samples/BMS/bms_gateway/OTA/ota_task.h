#ifndef OTA_TASK_H
#define OTA_TASK_H

#include "errcode.h"

/**
 * @brief OTA配置结构体
 */
typedef struct {
    char firmware_path[256]; // 固件文件路径
    char device_id[32];      // 目标设备ID
} ota_config_t;

/**
 * @brief 设置OTA服务器配置
 * @param ip 服务器IP地址
 * @param port 服务器端口
 * @param path 固件文件路径
 * @param device_id 目标设备ID
 * @return int 返回值，0表示成功
 */
int ota_set_config(const char *path, const char *device_id);

/**
 * @brief OTA准备函数
 * @param file_size 固件文件大小
 * @return errcode_t 错误码
 */
errcode_t ota_prepare(uint32_t file_size);

/**
 * @brief HTTP客户端获取升级文件并执行OTA
 * @param argument 参数（未使用）
 * @return int 返回值，0表示成功
 */
int http_clienti_get(const char *argument);

/**
 * @brief 启动OTA任务
 * @return int 返回值，0表示成功
 */
int ota_task_start(void);

/**
 * @brief 使用指定配置启动OTA任务
 * @param path 固件文件路径
 * @param device_id 目标设备ID
 * @return int 返回值，0表示成功
 */
int ota_task_start_with_config(const char *path, const char *device_id);

/**
 * @brief 检查设备ID是否匹配当前设备
 * @param device_id 要检查的设备ID
 * @return int 返回值，1表示匹配，0表示不匹配
 */
int ota_check_device_id(const char *device_id);

/**
 * @brief 重置OTA状态，清理升级相关的状态信息
 * @return int 返回值，0表示成功
 */


#endif // OTA_TASK_H