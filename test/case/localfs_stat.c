#include "test.h"
#include "vfs/fs/localfs.h"

#define LOCALFS_TEST_MOUNT_PATH "file:///foo/"

TEST_FIXTURE_SETUP(localfs)
{
    ASSERT_EQ_INT(0, vfs_init());

    /* Mount to #LOCALFS_TEST_MOUNT_PATH */
    {
        vfs_operations_t* fs = NULL;
        ASSERT_EQ_INT(vfs_make_local(&fs, "/"), 0);
        ASSERT_NE_PTR(fs, NULL);
        ASSERT_EQ_INT(vfs_mount(LOCALFS_TEST_MOUNT_PATH, fs), 0);
    }
}

TEST_FIXTURE_TEARDOWN(localfs)
{
    vfs_exit();
}

TEST_F(localfs, stat)
{
    vfs_operations_t* vfs = vfs_visitor_instance();
    ASSERT_NE_PTR(vfs, NULL);

    vfs_str_t file_path = vfs_str_from1(LOCALFS_TEST_MOUNT_PATH);
#if !defined(_WIN32)
    vfs_str_chop(&file_path, 1);
#endif
    vfs_str_append2(&file_path, &g_exe_path);

    vfs_stat_t info;
    ASSERT_EQ_INT(vfs->stat(vfs, file_path.str, &info), 0);

    ASSERT_EQ_UINT64(info.st_mode, VFS_S_IFREG);

    vfs_str_exit(&file_path);
}
