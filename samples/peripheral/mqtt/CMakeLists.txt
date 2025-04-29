# 设置源文件列表
# 包含WiFi连接、MQTT示例、蜂鸣器PWM、cJSON解析、
# 环境传感器(AHT20)和OLED显示(SSD1306)相关源文件
set(SOURCES_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/wifi/wifi_connect.c
    ${CMAKE_CURRENT_SOURCE_DIR}/mqtt_demo.c
    ${CMAKE_CURRENT_SOURCE_DIR}/beep/pwm_demo.c
    ${CMAKE_CURRENT_SOURCE_DIR}/cjson/cjson_demo.c
    ${CMAKE_CURRENT_SOURCE_DIR}/environment/aht20_test.c
    ${CMAKE_CURRENT_SOURCE_DIR}/environment/aht20.c
    ${CMAKE_CURRENT_SOURCE_DIR}/environment/ssd1306_fonts.c
    ${CMAKE_CURRENT_SOURCE_DIR}/environment/ssd1306.c
)

# 设置头文件目录列表
# 包含各个模块的头文件路径
set(HEADER_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/wifi 
    ${CMAKE_CURRENT_SOURCE_DIR}/cjson
    ${CMAKE_CURRENT_SOURCE_DIR}/environment
    ${CMAKE_CURRENT_SOURCE_DIR}/beep
)

# 将本目录的源文件添加到父作用域的SOURCES变量
# 将本目录的头文件路径添加到父作用域的PUBLIC_HEADER变量
set(SOURCES "${SOURCES}" ${SOURCES_LIST} PARENT_SCOPE)
set(PUBLIC_HEADER "${PUBLIC_HEADER}" ${HEADER_LIST} PARENT_SCOPE)
