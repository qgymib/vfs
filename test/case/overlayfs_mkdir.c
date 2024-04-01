#include <string.h>
#include "test.h"
#include "vfs/fs/localfs.h"
#include "vfs/fs/overlayfs.h"
#include "utils/defs.h"
#include "utils/overlayfs_builder.h"

#define TEST_OVERLAY_MOUNT_PATH "file:///test/overlay/"

static overlayfs_quick_t* s_test_overlayfs_mkdir = NULL;

TEST_FIXTURE_SETUP(overlayfs)
{
    ASSERT_EQ_INT(0, vfs_init());

    static vfs_fsbuilder_item_t s_lower_items[] = {
        { "/foo/bar", VFS_S_IFDIR },
        { NULL, VFS_S_IFDIR },
    };
    static vfs_fsbuilder_item_t s_upper_items[] = {
        { "/foo/bar.whiteout", VFS_S_IFDIR },
        { NULL, VFS_S_IFDIR },
    };
    s_test_overlayfs_mkdir = vfs_overlayfs_quick_make(TEST_OVERLAY_MOUNT_PATH,
        s_lower_items, s_upper_items);
    ASSERT_NE_PTR(s_test_overlayfs_mkdir, NULL);
}

TEST_FIXTURE_TEARDOWN(overlayfs)
{
    vfs_overlayfs_quick_exit(s_test_overlayfs_mkdir);
    s_test_overlayfs_mkdir = NULL;

    vfs_exit();
}

TEST_F(overlayfs, mkdir)
{
    vfs_operations_t* vfs = vfs_visitor_instance();
    ASSERT_NE_PTR(vfs, NULL);

    vfs_stat_t info;
    {
        const char* path = TEST_OVERLAY_MOUNT_PATH "test_dir";
        ASSERT_EQ_INT(vfs->mkdir(vfs, path), 0);
        ASSERT_EQ_INT(vfs->stat(vfs, path, &info), 0);
        ASSERT_EQ_UINT64(info.st_mode, VFS_S_IFDIR);
    }

    {
        vfs_operations_t* lower = s_test_overlayfs_mkdir->fs_lower;
        const char* path = "/test_dir";
        ASSERT_EQ_INT(lower->stat(lower, path, &info), VFS_ENOENT);
    }

    {
        vfs_operations_t* upper = s_test_overlayfs_mkdir->fs_upper;
        const char* path = "/test_dir";
        ASSERT_EQ_INT(upper->stat(upper, path, &info), 0);
        ASSERT_EQ_UINT64(info.st_mode, VFS_S_IFDIR);
    }
}
