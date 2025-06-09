if(DEFINED CONFIG_SAMPLE_SUPPORT_CLIENT)
    add_subdirectory_if_exist(client_test)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_SERVER)
    add_subdirectory_if_exist(server_test)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_HYBRID)
    add_subdirectory_if_exist(my_sle_hybrid)
endif()


set(PUBLIC_HEADER "${PUBLIC_HEADER}" ${HEADER_LIST} PARENT_SCOPE)
set(SOURCES "${SOURCES}" PARENT_SCOPE)