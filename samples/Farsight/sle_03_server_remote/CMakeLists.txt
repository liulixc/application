set(SOURCES_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/bsp_ina219.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/hal_bsp_dc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/hal_bsp_ssd1306.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/bsp_init.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/sle_server.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/remoteInfrared.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/hal_bsp_pcf8574.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/sle_server_adv.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/Track.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/pid.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sle_remote_server.c
)
set(HEADER_LIST ${CMAKE_CURRENT_SOURCE_DIR}/bsp)

set(SOURCES "${SOURCES}" ${SOURCES_LIST} PARENT_SCOPE)
set(PUBLIC_HEADER "${PUBLIC_HEADER}" ${HEADER_LIST} PARENT_SCOPE)

