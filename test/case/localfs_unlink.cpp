#include "test.h"
#include "vfs/fs/localfs.h"

#define LOCALFS_TEST_MOUNT_PATH "file:///foo/bar"

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

TEST_F(localfs, unlink)
{
    vfs_operations_t* vfs = vfs_visitor();
    ASSERT_NE_PTR(vfs, NULL);

    vfs_str_t file_path = vfs_str_from1(LOCALFS_TEST_MOUNT_PATH);
#if defined(_WIN32)
    vfs_str_append1(&file_path, "/");
#endif
    vfs_str_append2(&file_path, &g_cwd_path);
    vfs_str_append1(&file_path, "/localfs_rm");

    uintptr_t fh;
    ASSERT_EQ_INT(vfs->open(vfs, &fh, file_path.str, VFS_O_WRONLY | VFS_O_CREATE), 0);
    ASSERT_EQ_INT(vfs->write(vfs, fh, "hello", 5), 5);
    ASSERT_EQ_INT(vfs->close(vfs, fh), 0);

    vfs_stat_t info;
    ASSERT_EQ_INT(vfs->stat(vfs, file_path.str, &info), 0);
    ASSERT_EQ_UINT64(info.st_mode, VFS_S_IFREG);
    ASSERT_EQ_UINT64(info.st_size, 5);

    ASSERT_EQ_INT(vfs->unlink(vfs, file_path.str), 0);
    ASSERT_EQ_INT(vfs->stat(vfs, file_path.str, &info), VFS_ENOENT);

    vfs_str_exit(&file_path);
}
