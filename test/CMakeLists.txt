
add_executable(vfs_test
    case/localfs_ls.cpp
    case/localfs_mkdir.cpp
    case/localfs_read.cpp
    case/localfs_rm.cpp
    case/localfs_seek.cpp
    case/localfs_stat.cpp
    utils/cwdpath.c
    utils/exepath.c
    utils/file.c
    main.c
    test.c
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
