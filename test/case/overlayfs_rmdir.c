#include <string.h>
#include "test.h"
#include "vfs/fs/localfs.h"
#include "vfs/fs/overlayfs.h"
#include "utils/defs.h"
#include "utils/overlayfs_builder.h"

#define TEST_OVERLAY_MOUNT_PATH "file:///test/overlay/"

static overlayfs_quick_t* s_test_overlayfs_rmdir = NULL;

TEST_FIXTURE_SETUP(overlayfs)
{
    ASSERT_EQ_INT(0, vfs_init());

    static vfs_fsbuilder_item_t s_lower_items[] = {
        { "/foo/bar", VFS_S_IFDIR },
        { NULL, VFS_S_IFDIR },
    };
    static vfs_fsbuilder_item_t s_upper_items[] = {
        { NULL, VFS_S_IFDIR },
    };
    s_test_overlayfs_rmdir = vfs_overlayfs_quick_make(TEST_OVERLAY_MOUNT_PATH,
        s_lower_items, s_upper_items);
    ASSERT_NE_PTR(s_test_overlayfs_rmdir, NULL);
}

TEST_FIXTURE_TEARDOWN(overlayfs)
{
    vfs_overlayfs_quick_exit(s_test_overlayfs_rmdir);
    s_test_overlayfs_rmdir = NULL;

    vfs_exit();
}

TEST_F(overlayfs, rmdir)
{
    vfs_operations_t* vfs = vfs_visitor();
    ASSERT_NE_PTR(vfs, NULL);

    const char* path = TEST_OVERLAY_MOUNT_PATH "foo/bar";
    ASSERT_EQ_INT(vfs->rmdir(vfs, path), 0);

    vfs_stat_t info;
    ASSERT_EQ_INT(vfs->stat(vfs, path, &info), VFS_ENOENT);
    
    {
        vfs_operations_t* fs = s_test_overlayfs_rmdir->fs_lower;
        ASSERT_EQ_INT(fs->stat(fs, "/foo/bar", &info), 0);
        ASSERT_EQ_UINT64(info.st_mode, VFS_S_IFDIR);
    }

    {
        vfs_operations_t* fs = s_test_overlayfs_rmdir->fs_upper;
        ASSERT_EQ_INT(fs->stat(fs, "/foo/bar.whiteout", &info), 0);
        ASSERT_EQ_UINT64(info.st_mode, VFS_S_IFDIR);
    }
}
