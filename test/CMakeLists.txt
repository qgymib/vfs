
add_executable(vfs_test
    case/localfs.c
    case/localfs_ls.cpp
    case/localfs_mount.c
    case/memfs.c
    case/nullfs.c
    case/overlayfs.c
    case/overlayfs_ls.cpp
    case/overlayfs_mkdir.c
    case/overlayfs_read.c
    case/overlayfs_rmdir.c
    case/overlayfs_stat.c
    case/overlayfs_truncate.c
    case/overlayfs_unlink.c
    case/overlayfs_write.c
    generic/__init__.c
    generic/check_root.c
    generic/mkdir_parent_not_exist.c
    generic/mkdir_rmdir_in_root.c
    generic/open_parent_not_exist.c
    generic/open_unlink_in_root.c
    generic/rmdir_non_empty.c
    generic/rmdir_type_mismatch.c
    generic/truncate_larger_and_seek.c
    generic/truncate_smaller_and_seek.c
    generic/unlink_type_mismatch.c
    utils/cwdpath.c
    utils/exepath.c
    utils/fsbuilder.c
    utils/overlayfs_builder.c
    main.c
    test.c
)

if (VFS_ASAN)
    vfs_setup_asan(vfs_test)
endif ()
if (VFS_GCOV)
    setup_target_for_coverage_lcov(
        NAME vfs_coverage
        EXECUTABLE vfs_test
        EXCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/*" "/usr/include/c++/*"
    )
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
