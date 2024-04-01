#include <string.h>
#include "test.h"
#include "utils/defs.h"
#include "utils/overlayfs_builder.h"

#define TEST_OVERLAY_MOUNT_PATH "file:///test/overlay"

static overlayfs_quick_t* s_test_overlayfs_stat = NULL;

TEST_FIXTURE_SETUP(overlayfs)
{
    ASSERT_EQ_INT(0, vfs_init());

    static vfs_fsbuilder_item_t s_lower_items[] = {
        { "/foo/bar", VFS_S_IFDIR },
        { "/foo/test.txt", VFS_S_IFREG },
        { "/foo/bar/test.txt", VFS_S_IFREG },
        { "/foo/bar1", VFS_S_IFDIR },
        { NULL, VFS_S_IFDIR },
    };
    static vfs_fsbuilder_item_t s_upper_items[] = {
        { "/foo/bar.whiteout", VFS_S_IFDIR },
        { "/foo/test.txt.whiteout", VFS_S_IFREG },
        { NULL, VFS_S_IFDIR },
    };
    s_test_overlayfs_stat = vfs_overlayfs_quick_make(TEST_OVERLAY_MOUNT_PATH,
        s_lower_items, s_upper_items);
    ASSERT_NE_PTR(s_test_overlayfs_stat, NULL);
}

TEST_FIXTURE_TEARDOWN(overlayfs)
{
    vfs_overlayfs_quick_exit(s_test_overlayfs_stat);
    s_test_overlayfs_stat = NULL;

    vfs_exit();
}

TEST_F(overlayfs, stat)
{
    vfs_stat_t info;
    vfs_operations_t* fs = vfs_visitor_instance();
    ASSERT_NE_PTR(fs, NULL);

    /* This directory is not whiteout. */
    {
        const char* path = TEST_OVERLAY_MOUNT_PATH "/foo/bar1";
        ASSERT_EQ_INT(fs->stat(fs, path, &info), 0);
        ASSERT_EQ_UINT64(info.st_mode, VFS_S_IFDIR);
    }

    /* This directory is whiteout. */
    {
        const char* path = TEST_OVERLAY_MOUNT_PATH "/foo/bar";
        ASSERT_EQ_INT(fs->stat(fs, path, &info), VFS_ENOENT);
    }

    /* The parent directory is whiteout. */
    {
        const char* path = TEST_OVERLAY_MOUNT_PATH "/foo/bar/test.txt";
        ASSERT_EQ_INT(fs->stat(fs, path, &info), VFS_ENOENT);
    }

    /* This file is directly whiteout. */
    {
        const char* path = TEST_OVERLAY_MOUNT_PATH "/foo/test.txt";
        ASSERT_EQ_INT(fs->stat(fs, path, &info), VFS_ENOENT);
    }
}
