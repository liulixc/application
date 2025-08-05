set(SOURCES_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/bms_slave_task/bms_slave.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sle_server/sle_hybrid.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sle_server/sle_uuid_server.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sle_server/sle_uuid_client.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sle_server/sle_callbacks.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sle_server/sle_server_adv.c
    ${CMAKE_CURRENT_SOURCE_DIR}/OTA/ota_task.c
    ${CMAKE_CURRENT_SOURCE_DIR}/OTA/wifi_connect.c
    
)


set(HEADER_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/sle_server
    ${CMAKE_CURRENT_SOURCE_DIR}/bms_slave_task
    ${CMAKE_CURRENT_SOURCE_DIR}/OTA
)

set(SOURCES "${SOURCES}" ${SOURCES_LIST} PARENT_SCOPE)
set(PUBLIC_HEADER "${PUBLIC_HEADER}" ${HEADER_LIST} PARENT_SCOPE)
