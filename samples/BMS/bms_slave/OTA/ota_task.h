#ifndef OTA_TASK_H
#define OTA_TASK_H

#include "errcode.h"

/**
 * @brief OTA准备函数
 * @param file_size 升级文件大小
 * @return errcode_t 错误码
 */
errcode_t ota_prepare(uint32_t file_size);

/**
 * @brief HTTP客户端获取升级文件并执行OTA
 * @param argument 参数（未使用）
 * @return int 返回值，0表示成功，-1表示失败
 */
int http_clienti_get(const char *argument);

/**
 * @brief 启动OTA任务
 * @return int 返回值，0表示成功，-1表示失败
 */
int ota_task_start(void);

/**
 * @brief 更新固件下载路径
 * @param firmware_path 新的固件路径
 */
void ota_update_firmware_path(const char *firmware_path);

#endif // OTA_TASK_H