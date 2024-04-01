#include "test.h"
#include "vfs/fs/nullfs.h"

#define LOCALFS_TEST_MOUNT_PATH "file:///nullfs"

TEST_FIXTURE_SETUP(nullfs)
{
    ASSERT_EQ_INT(0, vfs_init());

    vfs_operations_t* nullfs = NULL;
    ASSERT_EQ_INT(vfs_make_null(&nullfs), 0);
    ASSERT_EQ_INT(vfs_mount(LOCALFS_TEST_MOUNT_PATH, nullfs), 0);
}

TEST_FIXTURE_TEARDOWN(nullfs)
{
    vfs_exit();
}

static int _test_nullfs_ls_cb(const char* name, const vfs_stat_t* stat, void* data)
{
    (void)name; (void)stat;
    size_t* cnt = data;
    *cnt += 1;
    return 0;
}

TEST_F(nullfs, ls)
{
    vfs_operations_t* vfs = vfs_visitor_instance();
    ASSERT_NE_PTR(vfs, NULL);

    {
        size_t cnt = 0;
        const char* path = LOCALFS_TEST_MOUNT_PATH;
        ASSERT_EQ_INT(vfs->ls(vfs, path, _test_nullfs_ls_cb, &cnt), 0);
        ASSERT_EQ_SIZE(cnt, 0);
    }

    {
        size_t cnt = 0;
        const char* path = LOCALFS_TEST_MOUNT_PATH "/foo";
        ASSERT_EQ_INT(vfs->ls(vfs, path, _test_nullfs_ls_cb, &cnt), VFS_ENOENT);
    }
}

TEST_F(nullfs, stat)
{
    vfs_operations_t* vfs = vfs_visitor_instance();
    ASSERT_NE_PTR(vfs, NULL);

    vfs_stat_t info;
    {
        const char* path = LOCALFS_TEST_MOUNT_PATH;
        ASSERT_EQ_INT(vfs->stat(vfs, path, &info), 0);
        ASSERT_EQ_UINT64(info.st_mode, VFS_S_IFDIR);
    }
    {
        const char* path = LOCALFS_TEST_MOUNT_PATH "/foo";
        ASSERT_EQ_INT(vfs->stat(vfs, path, &info), VFS_ENOENT);
    }
}

TEST_F(nullfs, open_close)
{
    vfs_operations_t* vfs = vfs_visitor_instance();
    ASSERT_NE_PTR(vfs, NULL);

    uintptr_t fh = 0;
    {
        const char* path = LOCALFS_TEST_MOUNT_PATH;
        ASSERT_EQ_INT(vfs->open(vfs, &fh, path, VFS_O_CREATE), VFS_EISDIR);
    }
    {
        const char* path = LOCALFS_TEST_MOUNT_PATH "/foo";
        ASSERT_EQ_INT(vfs->open(vfs, &fh, path, VFS_O_WRONLY), VFS_ENOENT);
    }
    {
        const char* path = LOCALFS_TEST_MOUNT_PATH "/foo";
        ASSERT_EQ_INT(vfs->open(vfs, &fh, path, VFS_O_WRONLY | VFS_O_CREATE), 0);
        ASSERT_EQ_INT(vfs->close(vfs, fh), 0);
    }
}

TEST_F(nullfs, seek)
{
    vfs_operations_t* vfs = vfs_visitor_instance();
    ASSERT_NE_PTR(vfs, NULL);

    uintptr_t fh = 0;
    {
        const char* path = LOCALFS_TEST_MOUNT_PATH "/foo";
        ASSERT_EQ_INT(vfs->open(vfs, &fh, path, VFS_O_RDWR | VFS_O_CREATE), 0);
        ASSERT_EQ_INT(vfs->write(vfs, fh, "dummy", 5), 5);
        ASSERT_EQ_INT64(vfs->seek(vfs, fh, -1, VFS_SEEK_END), 0);
        ASSERT_EQ_INT(vfs->close(vfs, fh), 0);
    }
}
