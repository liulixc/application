#===============================================================================
# @brief    cmake file
# Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2023-2023. All rights reserved.
#===============================================================================
set(COMPONENT_NAME "samples") # 设置组件名称

set(SOURCES
) # 源文件列表（此处为空，由子目录补充）

set(PUBLIC_HEADER
) # 公共头文件列表

# 根据配置选项，选择性添加子目录
if(DEFINED CONFIG_ENABLE_BT_SAMPLE)
    add_subdirectory_if_exist(bt) # 添加蓝牙样例
endif()
if(DEFINED CONFIG_ENABLE_PERIPHERAL_SAMPLE)
    add_subdirectory_if_exist(peripheral) # 添加外设样例
endif()
if(DEFINED CONFIG_ENABLE_WIFI_SAMPLE)
    add_subdirectory_if_exist(wifi) # 添加WiFi样例
endif()
if(DEFINED CONFIG_ENABLE_PRODUCTS_SAMPLE)
    add_subdirectory_if_exist(products) # 添加产品样例
endif()
if(DEFINED CONFIG_ENABLE_RADAR_SAMPLE)
    add_subdirectory_if_exist(radar) # 添加雷达样例
endif()
if(DEFINED CONFIG_ENABLE_NFC_SAMPLE)
    add_subdirectory_if_exist(nfc) # 添加NFC样例
endif()
if(DEFINED CONFIG_ENABLE_FARSIGHT_SAMPLE)
    add_subdirectory_if_exist(Farsight) # 添加华清远见样例
endif()
if(DEFINED CONFIG_ENABLE_BMS_SAMPLE)
    add_subdirectory_if_exist(BMS) # 添加电池管理样例
endif()

if(DEFINED CONFIG_ENABLE_SLE_GATE_SAMPLE)
    add_subdirectory_if_exist(sle_gate) # 添加SLE网关样例
endif()

if(DEFINED CONFIG_ENABLE_SLE_HYBRID)
    add_subdirectory_if_exist(hybrid_demo) # 添加HiLink样例
endif()

set(PRIVATE_HEADER
) # 私有头文件列表

set(PRIVATE_DEFINES
) # 私有宏定义

set(PUBLIC_DEFINES
) # 公共宏定义

# 用于添加公共编译选项，如 -include xxx
set(COMPONENT_PUBLIC_CCFLAGS
)

set(COMPONENT_CCFLAGS
)

set(WHOLE_LINK
    true
) # 是否整体链接

set(MAIN_COMPONENT
    false
) # 是否为主组件

# 安装各子目录的SDK文件，便于开发包分发
install_sdk("${CMAKE_CURRENT_SOURCE_DIR}/peripheral" "*")
install_sdk("${CMAKE_CURRENT_SOURCE_DIR}/products" "*")
install_sdk("${CMAKE_CURRENT_SOURCE_DIR}/wifi/sta_sample" "*")
install_sdk("${CMAKE_CURRENT_SOURCE_DIR}/wifi/alilink_sample" "*")
install_sdk("${CMAKE_CURRENT_SOURCE_DIR}/wifi/softap_sample" "*")
install_sdk("${CMAKE_CURRENT_SOURCE_DIR}/wifi/ohos_connect" "*")
install_sdk("${CMAKE_CURRENT_SOURCE_DIR}/wifi/hilink_indie_upgrade" "*")
install_sdk("${CMAKE_CURRENT_SOURCE_DIR}/wifi/ble_wifi_cfg_sample" "*")
install_sdk("${CMAKE_CURRENT_SOURCE_DIR}/radar" "*")
install_sdk("${CMAKE_CURRENT_SOURCE_DIR}/nfc" "*")
install_sdk("${CMAKE_CURRENT_SOURCE_DIR}/bt/ble" "*")
install_sdk("${CMAKE_CURRENT_SOURCE_DIR}/bt/sle" "*")
install_sdk("${CMAKE_CURRENT_SOURCE_DIR}/Farsight" "*")
install_sdk("${CMAKE_CURRENT_SOURCE_DIR}/BMS" "*")
install_sdk("${CMAKE_CURRENT_SOURCE_DIR}/sle_gate" "*")
install_sdk("${CMAKE_CURRENT_SOURCE_DIR}/hybrid_demo" "*")

build_component() # 构建该组件
