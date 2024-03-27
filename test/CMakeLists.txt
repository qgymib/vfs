
add_executable(vfs_test
    case/init_and_exit.c
    main.c
)

if (VFS_ASAN)
    vfs_setup_asan(vfs_test)
endif ()

vfs_setup_target_wall(vfs_test)

target_include_directories(vfs_test
    PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(vfs_test
    PRIVATE
        cutest
        vfs
)

add_test(NAME vfs_test
    COMMAND vfs_test
)
