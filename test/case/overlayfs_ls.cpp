#include <map>
#include <string>
#include "test.h"
#include "vfs/fs/localfs.h"
#include "vfs/fs/overlayfs.h"
#include "utils/defs.h"
#include "utils/overlayfs_builder.h"

#define TEST_OVERLAY_MOUNT_PATH "file:///test/overlay"

typedef std::map<std::string, vfs_stat_t> ItemMap;

static overlayfs_quick_t* s_test_overlayfs_ls = NULL;

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
        { "/foo/bar2", VFS_S_IFREG },
        { NULL, VFS_S_IFDIR },
    };
    s_test_overlayfs_ls = vfs_overlayfs_quick_make(TEST_OVERLAY_MOUNT_PATH,
        s_lower_items, s_upper_items);
    ASSERT_NE_PTR(s_test_overlayfs_ls, NULL);
}

TEST_FIXTURE_TEARDOWN(overlayfs)
{
    vfs_overlayfs_quick_exit(s_test_overlayfs_ls);
    s_test_overlayfs_ls = NULL;

    vfs_exit();
}

static int _localfs_test_ls_cb(const char* name, const vfs_stat_t* stat, void* data)
{
    ItemMap* items = static_cast<ItemMap*>(data);
    items->insert(ItemMap::value_type(name, *stat));
    return 0;
}

TEST_F(overlayfs, ls)
{
    vfs_operations_t* vfs = vfs_visitor_instance();
    ASSERT_NE_PTR(vfs, NULL);

    ItemMap items;
    ASSERT_EQ_INT(vfs->ls(vfs, TEST_OVERLAY_MOUNT_PATH "/foo", _localfs_test_ls_cb, &items), 0);

    ASSERT_EQ_SIZE(items.size(), 2);
}
