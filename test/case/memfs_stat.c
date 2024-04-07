#include "test.h"
#include "vfs/fs/memfs.h"
#include "utils/fsbuilder.h"

#define TEST_MEMFS_MOUNT_POINT  "/memfs"

TEST_FIXTURE_SETUP(memfs)
{
    ASSERT_EQ_INT(0, vfs_init());

    {
        vfs_operations_t* memfs = NULL;
        ASSERT_EQ_INT(vfs_make_memory(&memfs), 0);
        ASSERT_NE_PTR(memfs, NULL);
        ASSERT_EQ_INT(vfs_mount(TEST_MEMFS_MOUNT_POINT, memfs), 0);
    }

    vfs_operations_t* vfs = vfs_visitor_instance();
    static vfs_fsbuilder_item_t s_items[] = {
        { "/foo", VFS_S_IFDIR },
        { "/bar", VFS_S_IFREG },
        { NULL, 0 },
    };
    vfs_test_fs_build(vfs, s_items, TEST_MEMFS_MOUNT_POINT);
}

TEST_FIXTURE_TEARDOWN(memfs)
{
    vfs_exit();
}

TEST_F(memfs, stat)
{
    vfs_operations_t* vfs = vfs_visitor_instance();
    ASSERT_NE_PTR(vfs, NULL);

    vfs_stat_t info;
    {
        const char* path = TEST_MEMFS_MOUNT_POINT "/foo";
        ASSERT_EQ_INT(vfs->stat(vfs, path, &info), 0);
        ASSERT_EQ_UINT64(info.st_mode, VFS_S_IFDIR);
    }
    {
        const char* path = TEST_MEMFS_MOUNT_POINT "/bar";
        ASSERT_EQ_INT(vfs->stat(vfs, path, &info), 0);
        ASSERT_EQ_UINT64(info.st_mode, VFS_S_IFREG);
    }
}
