
add_executable(vfs_test
    case/localfs_ls.cpp
    case/localfs_mkdir.c
    case/localfs_mount.c
    case/localfs_read.c
    case/localfs_seek.c
    case/localfs_stat.c
    case/localfs_unlink.c
    case/nullfs.c
    case/overlayfs_ls.cpp
    case/overlayfs_mkdir.c
    case/overlayfs_read.c
    case/overlayfs_rmdir.c
    case/overlayfs_stat.c
    case/overlayfs_unlink.c
    case/overlayfs_write.c
    utils/cwdpath.c
    utils/exepath.c
    utils/file.c
    utils/overlayfs_builder.c
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
