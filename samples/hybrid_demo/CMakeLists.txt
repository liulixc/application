if(DEFINED CONFIG_SAMPLE_SUPPORT_GATEWAY)
    add_subdirectory_if_exist(gateway)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_SERVER)
    add_subdirectory_if_exist(server_test)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_HYBRID)
    add_subdirectory_if_exist(my_sle_hybrid)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_MESH)
    add_subdirectory_if_exist(sle_03_mesh_node)
endif()

set(PUBLIC_HEADER "${PUBLIC_HEADER}" ${HEADER_LIST} PARENT_SCOPE)
set(SOURCES "${SOURCES}" PARENT_SCOPE)