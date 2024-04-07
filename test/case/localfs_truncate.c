#include "test.h"
#include "vfs/fs/localfs.h"
#include "utils/fsbuilder.h"

#define LOCALFS_TEST_MOUNT_PATH "/localfs"

TEST_FIXTURE_SETUP(localfs)
{
    ASSERT_EQ_INT(0, vfs_init());

    /* Mount to #LOCALFS_TEST_MOUNT_PATH */
    vfs_operations_t* fs = NULL;
    {
        ASSERT_EQ_INT(vfs_make_local(&fs, g_cwd_path.str), 0);
        ASSERT_NE_PTR(fs, NULL);
        ASSERT_EQ_INT(vfs_mount(LOCALFS_TEST_MOUNT_PATH, fs), 0);
    }

    ASSERT_EQ_INT(vfs_file_write(fs, "/hello", VFS_O_WRONLY | VFS_O_CREATE, "dummy", 5), 5);
}

TEST_FIXTURE_TEARDOWN(localfs)
{
    vfs_exit();
}

TEST_F(localfs, truncate)
{
    vfs_operations_t* vfs = vfs_visitor_instance();
    ASSERT_NE_PTR(vfs, NULL);

    vfs_stat_t info;
    const char* path = LOCALFS_TEST_MOUNT_PATH "/hello";

    {
        ASSERT_EQ_INT(vfs->stat(vfs, path, &info), 0);
        ASSERT_EQ_UINT64(info.st_size, 5);
    }

    {
        uintptr_t fh = 0;
        ASSERT_EQ_INT(vfs->open(vfs, &fh, path, VFS_O_WRONLY), 0);
        ASSERT_EQ_INT(vfs->truncate(vfs, fh, 1), 0);
        ASSERT_EQ_INT(vfs->close(vfs, fh), 0);
    }

    {
        ASSERT_EQ_INT(vfs->stat(vfs, path, &info), 0);
        ASSERT_EQ_UINT64(info.st_size, 1);
    }
}
