/*
 * Copyright (C) 2024 HiHope Open Source Organization.
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

/**
 * @defgroup SLE_Distribute_Network_Server_adv SLE分布式网络服务端广播模块
 * @ingroup SLE_Distribute_Network_Server
 * @brief SLE分布式网络服务端广播相关定义和接口。
 * @{
 */

#ifndef SLE_DISTRIBUTE_NETWORK_SERVER_ADV_H
#define SLE_DISTRIBUTE_NETWORK_SERVER_ADV_H

#include <stdint.h>

/**
 * @brief SLE广播通用数据结构。
 * 用于描述广播数据的基本格式，包括长度、类型和值。
 * 
 * length: 数据长度，单位为字节。
 * type: 数据类型，参见sle_adv_data_type。
 * value: 数据值，实际应用中可为指针或数组，视具体实现而定。
 */
struct sle_adv_common_value {
    uint8_t length; ///< 数据长度
    uint8_t type;   ///< 数据类型
    uint8_t value;  ///< 数据值
};

/**
 * @brief SLE广播信道映射枚举。
 * 用于指定广播使用的物理信道。
 * 
 * SLE_ADV_CHANNEL_MAP_77: 使用77信道进行广播。
 * SLE_ADV_CHANNEL_MAP_78: 使用78信道进行广播。
 * SLE_ADV_CHANNEL_MAP_79: 使用79信道进行广播。
 * SLE_ADV_CHANNEL_MAP_DEFAULT: 默认使用77、78、79三个信道。
 */
typedef enum {
    SLE_ADV_CHANNEL_MAP_77 = 0x01,   ///< 使用77信道
    SLE_ADV_CHANNEL_MAP_78 = 0x02,   ///< 使用78信道
    SLE_ADV_CHANNEL_MAP_79 = 0x04,   ///< 使用79信道
    SLE_ADV_CHANNEL_MAP_DEFAULT = 0x07 ///< 默认全部信道
} sle_adv_channel_map;

/**
 * @brief SLE广播数据类型枚举。
 * 用于标识广播数据的具体内容类型。
 */
typedef enum {
    SLE_ADV_DATA_TYPE_DISCOVERY_LEVEL = 0x01,                         /*!< 发现等级。用于指示设备的发现优先级或可见性等级。 */
    SLE_ADV_DATA_TYPE_ACCESS_MODE = 0x02,                             /*!< 接入层能力。描述设备支持的接入模式或能力。 */
    SLE_ADV_DATA_TYPE_SERVICE_DATA_16BIT_UUID = 0x03,                 /*!< 标准服务数据信息。包含16位UUID对应的服务相关数据。 */
    SLE_ADV_DATA_TYPE_SERVICE_DATA_128BIT_UUID = 0x04,                /*!< 自定义服务数据信息。包含128位UUID对应的自定义服务数据。 */
    SLE_ADV_DATA_TYPE_COMPLETE_LIST_OF_16BIT_SERVICE_UUIDS = 0x05,    /*!< 完整标准服务标识列表。广播设备支持的全部16位标准服务UUID。 */
    SLE_ADV_DATA_TYPE_COMPLETE_LIST_OF_128BIT_SERVICE_UUIDS = 0x06,   /*!< 完整自定义服务标识列表。广播设备支持的全部128位自定义服务UUID。 */
    SLE_ADV_DATA_TYPE_INCOMPLETE_LIST_OF_16BIT_SERVICE_UUIDS = 0x07,  /*!< 部分标准服务标识列表。广播部分16位标准服务UUID。 */
    SLE_ADV_DATA_TYPE_INCOMPLETE_LIST_OF_128BIT_SERVICE_UUIDS = 0x08, /*!< 部分自定义服务标识列表。广播部分128位自定义服务UUID。 */
    SLE_ADV_DATA_TYPE_SERVICE_STRUCTURE_HASH_VALUE = 0x09,            /*!< 服务结构散列值。用于快速校验服务结构是否发生变化。 */
    SLE_ADV_DATA_TYPE_SHORTENED_LOCAL_NAME = 0x0A,                    /*!< 设备缩写本地名称。广播设备名称的缩写形式。 */
    SLE_ADV_DATA_TYPE_COMPLETE_LOCAL_NAME = 0x0B,                     /*!< 设备完整本地名称。广播设备的完整名称。 */
    SLE_ADV_DATA_TYPE_TX_POWER_LEVEL = 0x0C,                          /*!< 广播发送功率。指示设备广播时的发射功率级别。 */
    SLE_ADV_DATA_TYPE_SLB_COMMUNICATION_DOMAIN = 0x0D,                /*!< SLB通信域域名。用于标识设备所属的通信域。 */
    SLE_ADV_DATA_TYPE_SLB_MEDIA_ACCESS_LAYER_ID = 0x0E,               /*!< SLB媒体接入层标识。标识设备的媒体接入层ID。 */
    SLE_ADV_DATA_TYPE_EXTENDED = 0xFE,                                /*!< 数据类型扩展。用于扩展更多自定义数据类型。 */
    SLE_ADV_DATA_TYPE_MANUFACTURER_SPECIFIC_DATA = 0xFF               /*!< 厂商自定义信息。用于广播厂商自定义的专有数据。 */
} sle_adv_data_type;

/**
 * @brief SLE广播初始化接口。
 *
 * 用于配置和启动SLE分布式网络服务端的广播功能。
 *
 * @return
 *   - ERRCODE_SLE_SUCCESS：执行成功
 *   - ERRCODE_SLE_FAIL：执行失败
 */
errcode_t example_sle_server_adv_init(void);

#endif // SLE_DISTRIBUTE_NETWORK_SERVER_ADV_H

/**
 * @}
 */