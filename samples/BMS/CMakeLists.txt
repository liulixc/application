if(DEFINED CONFIG_SAMPLE_SUPPORT_BMS_GATEWAY)
    add_subdirectory_if_exist(bms_gateway)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_BMS_SLAVE)
    add_subdirectory_if_exist(bms_slave)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_SLE_UART_1_VS_8)
    add_subdirectory_if_exist(sle_uart_1_vs_8)
endif()


set(PUBLIC_HEADER "${PUBLIC_HEADER}" ${HEADER_LIST} PARENT_SCOPE)
set(SOURCES "${SOURCES}" PARENT_SCOPE)