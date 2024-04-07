#include "test.h"
#include "utils/overlayfs_builder.h"

#define TEST_OVERLAY_MOUNT_PATH "file:///test/overlayfs"

static overlayfs_quick_t* s_test_overlayfs_truncate = NULL;

TEST_FIXTURE_SETUP(overlayfs)
{
    ASSERT_EQ_INT(0, vfs_init());

    s_test_overlayfs_truncate = vfs_overlayfs_quick_make(TEST_OVERLAY_MOUNT_PATH,
        NULL, NULL);
    ASSERT_NE_PTR(s_test_overlayfs_truncate, NULL);

    ASSERT_EQ_INT(vfs_file_write(s_test_overlayfs_truncate->fs_lower, "/foo/bar", VFS_O_WRONLY | VFS_O_CREATE, "dummy", 5), 5);
}

TEST_FIXTURE_TEARDOWN(overlayfs)
{
    vfs_overlayfs_quick_exit(s_test_overlayfs_truncate);
    s_test_overlayfs_truncate = NULL;

    vfs_exit();
}

TEST_F(overlayfs, truncate)
{
    vfs_operations_t* vfs = vfs_visitor_instance();
    ASSERT_NE_PTR(vfs, NULL);

    uintptr_t fh = 0;
    const char* path = TEST_OVERLAY_MOUNT_PATH "/foo/bar";
    ASSERT_EQ_INT(vfs->open(vfs, &fh, path, VFS_O_WRONLY), 0);
    ASSERT_EQ_INT(vfs->truncate(vfs, fh, 1), 0);
    ASSERT_EQ_INT(vfs->close(vfs, fh), 0);

    vfs_stat_t info;
    ASSERT_EQ_INT(vfs->stat(vfs, path, &info), 0);
    ASSERT_EQ_UINT64(info.st_size, 1);
}
