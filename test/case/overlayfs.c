#include "test.h"
#include "vfs/fs/overlayfs.h"
#include "generic/__init__.h"
#include "utils/overlayfs_builder.h"

#define TEST_OVERLAY_MOUNT_PATH "file:///test/overlay/"

static overlayfs_quick_t* s_test_overlayfs = NULL;

TEST_FIXTURE_SETUP(overlayfs)
{
    ASSERT_EQ_INT(0, vfs_init());

    s_test_overlayfs = vfs_overlayfs_quick_make(TEST_OVERLAY_MOUNT_PATH,
        NULL, NULL);
    ASSERT_NE_PTR(s_test_overlayfs, NULL);
}

TEST_FIXTURE_TEARDOWN(overlayfs)
{
    vfs_overlayfs_quick_exit(s_test_overlayfs);
    s_test_overlayfs = NULL;

    vfs_exit();
}

TEST_F(overlayfs, generic)
{
    vfs_test_generic(s_test_overlayfs->fs_overlay);
}
