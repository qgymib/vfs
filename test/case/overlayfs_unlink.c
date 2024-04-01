#include "test.h"
#include "utils/overlayfs_builder.h"

#define TEST_OVERLAY_MOUNT_PATH "file:///test/overlayfs"

static overlayfs_quick_t* s_test_overlayfs_unlink = NULL;

TEST_FIXTURE_SETUP(overlayfs)
{
    ASSERT_EQ_INT(0, vfs_init());

    static vfs_fsbuilder_item_t s_lower_items[] = {
        { "/foo/bar", VFS_S_IFREG },
        { "/foo/hello", VFS_S_IFREG },
        { NULL, 0 },
    };
    static vfs_fsbuilder_item_t s_upper_items[] = {
        { "/foo/bar.whiteout", VFS_S_IFREG },
        { "/foo/world", VFS_S_IFREG },
        { NULL, 0 },
    };
    s_test_overlayfs_unlink = vfs_overlayfs_quick_make(TEST_OVERLAY_MOUNT_PATH,
        s_lower_items, s_upper_items);
    ASSERT_NE_PTR(s_test_overlayfs_unlink, NULL);
}

TEST_FIXTURE_TEARDOWN(overlayfs)
{
    vfs_overlayfs_quick_exit(s_test_overlayfs_unlink);
    s_test_overlayfs_unlink = NULL;

    vfs_exit();
}

TEST_F(overlayfs, unlink)
{
    vfs_stat_t info;
    vfs_operations_t* vfs = vfs_visitor();
    ASSERT_NE_PTR(vfs, NULL);

    /* unlink a whiteouted file. */
    {
        const char* path = TEST_OVERLAY_MOUNT_PATH "/foo/bar";
        ASSERT_EQ_INT(vfs->unlink(vfs, path), VFS_ENOENT);
    }

    /* unlink file in lower layer. */
    {
        const char* full_path = TEST_OVERLAY_MOUNT_PATH "/foo/hello";
        const char* path = "/foo/hello";
        const char* path_wh = "/foo/hello.whiteout";

        ASSERT_EQ_INT(vfs->unlink(vfs, full_path), 0);
        ASSERT_EQ_INT(vfs->stat(vfs, full_path, &info), VFS_ENOENT);

        vfs_operations_t* lower = s_test_overlayfs_unlink->fs_lower;
        ASSERT_EQ_INT(lower->stat(lower, path, &info), 0);
        ASSERT_EQ_UINT64(info.st_mode, VFS_S_IFREG);

        vfs_operations_t* upper = s_test_overlayfs_unlink->fs_upper;
        ASSERT_EQ_INT(upper->stat(upper, path_wh, &info), 0);
    }

    {
        const char* path = TEST_OVERLAY_MOUNT_PATH "/foo/world";

        ASSERT_EQ_INT(vfs->unlink(vfs, path), 0);
        ASSERT_EQ_INT(vfs->stat(vfs, path, &info), VFS_ENOENT);

        vfs_operations_t* upper = s_test_overlayfs_unlink->fs_upper;
        ASSERT_EQ_INT(upper->stat(upper, "/foo/world", &info), VFS_ENOENT);
    }
}
