#===============================================================================
# @brief    cmake file
# Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2023-2023. All rights reserved.
#===============================================================================

set(SOURCES "${SOURCES}"
    "${CMAKE_CURRENT_SOURCE_DIR}/sle_speed_server_adv.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/sle_speed_server.c"
    PARENT_SCOPE)

set(SOURCES "${SOURCES}" PARENT_SCOPE)
set(PUBLIC_HEADER "${PUBLIC_HEADER}" "${CMAKE_CURRENT_SOURCE_DIR}/inc" PARENT_SCOPE)