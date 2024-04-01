#include "test.h"
#include "utils/overlayfs_builder.h"
#include "utils/file.h"

#define TEST_OVERLAY_MOUNT_PATH "file:///test/overlayfs"

static overlayfs_quick_t* s_test_overlayfs_read = NULL;

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
        { "/foo/hello", VFS_S_IFREG },
        { NULL, 0 },
    };
    s_test_overlayfs_read = vfs_overlayfs_quick_make(TEST_OVERLAY_MOUNT_PATH,
        s_lower_items, s_upper_items);
    ASSERT_NE_PTR(s_test_overlayfs_read, NULL);

    vfs_test_write_file(s_test_overlayfs_read->fs_lower, "/foo/hello", "hello", 5);
    vfs_test_write_file(s_test_overlayfs_read->fs_upper, "/foo/hello", "world", 5);
}

TEST_FIXTURE_TEARDOWN(overlayfs)
{
    vfs_overlayfs_quick_exit(s_test_overlayfs_read);
    s_test_overlayfs_read = NULL;

    vfs_exit();
}

TEST_F(overlayfs, read)
{
    vfs_operations_t* vfs = vfs_visitor();
    ASSERT_NE_PTR(vfs, NULL);

    /* Open a file marked as delete. */
    {
        uintptr_t fh = 0;
        const char* path = TEST_OVERLAY_MOUNT_PATH "/foo/bar";
        ASSERT_EQ_INT(vfs->open(vfs, &fh, path, VFS_O_RDONLY), VFS_ENOENT);
    }

    /* Open a override file. */
    {
        const char* path = TEST_OVERLAY_MOUNT_PATH "/foo/hello";
        
        vfs_str_t dat = VFS_STR_INIT;
        vfs_test_read_file(vfs, path, &dat);

        ASSERT_EQ_INT(vfs_str_cmp1(&dat, "world"), 0);
        vfs_str_exit(&dat);
    }
}
