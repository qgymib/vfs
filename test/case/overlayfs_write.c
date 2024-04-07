#include "test.h"
#include "utils/overlayfs_builder.h"
#include "utils/fsbuilder.h"

#define TEST_OVERLAY_MOUNT_PATH "file:///test/overlayfs"

static overlayfs_quick_t* s_test_overlayfs_write = NULL;

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
        { NULL, 0 },
    };
    s_test_overlayfs_write = vfs_overlayfs_quick_make(TEST_OVERLAY_MOUNT_PATH,
        s_lower_items, s_upper_items);
    ASSERT_NE_PTR(s_test_overlayfs_write, NULL);

    vfs_file_write(s_test_overlayfs_write->fs_lower, "/foo/bar", VFS_O_WRONLY | VFS_O_CREATE, "bar", 3);
    vfs_file_write(s_test_overlayfs_write->fs_lower, "/foo/hello", VFS_O_WRONLY | VFS_O_CREATE, "hello", 5);
}

TEST_FIXTURE_TEARDOWN(overlayfs)
{
    vfs_overlayfs_quick_exit(s_test_overlayfs_write);
    s_test_overlayfs_write = NULL;

    vfs_exit();
}

TEST_F(overlayfs, write)
{
    vfs_operations_t* vfs = vfs_visitor_instance();
    ASSERT_NE_PTR(vfs, NULL);

    {
        vfs_str_t dat = VFS_STR_INIT;
        const char* path = TEST_OVERLAY_MOUNT_PATH "/foo/bar";
        vfs_file_write(vfs, path, VFS_O_WRONLY | VFS_O_CREATE, "world", 5);

        vfs_test_read_file(vfs, path, &dat);
        ASSERT_EQ_INT(vfs_str_cmp1(&dat, "world"), 0);

        vfs_str_reset(&dat);
        vfs_test_read_file(s_test_overlayfs_write->fs_upper, "/foo/bar", &dat);
        ASSERT_EQ_INT(vfs_str_cmp1(&dat, "world"), 0);

        vfs_stat_t info;
        {
            vfs_operations_t* fs = s_test_overlayfs_write->fs_upper;
            ASSERT_EQ_INT(fs->stat(fs, "/foo/bar.whiteout", &info), VFS_ENOENT);
        }

        vfs_str_exit(&dat);
    }
}
