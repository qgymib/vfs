#include "test.h"
#include "vfs/fs/localfs.h"

#define LOCALFS_TEST_MOUNT_PATH "/foo/"

TEST_FIXTURE_SETUP(localfs)
{
    ASSERT_EQ_INT(0, vfs_init());

    /* Mount to #LOCALFS_TEST_MOUNT_PATH */
    {
        vfs_operations_t* fs = NULL;
        ASSERT_EQ_INT(vfs_make_local(&fs, g_cwd_path.str), 0);
        ASSERT_NE_PTR(fs, NULL);
        ASSERT_EQ_INT(vfs_mount(LOCALFS_TEST_MOUNT_PATH, fs), 0);
    }
}

TEST_FIXTURE_TEARDOWN(localfs)
{
    vfs_exit();
}

TEST_F(localfs, mkdir)
{
    vfs_operations_t* vfs = vfs_visitor();
    ASSERT_NE_PTR(vfs, NULL);

    const char* path = LOCALFS_TEST_MOUNT_PATH "test_mkdir";
    ASSERT_EQ_INT(vfs->mkdir(vfs, path), 0);

    vfs_stat_t info;
    ASSERT_EQ_INT(vfs->stat(vfs, path, &info), 0);
    ASSERT_EQ_UINT64(info.st_mode, VFS_S_IFDIR);

    ASSERT_EQ_INT(vfs->rmdir(vfs, path), 0);
}
