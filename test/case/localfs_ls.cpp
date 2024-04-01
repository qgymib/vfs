#include "test.h"
#include "vfs/fs/localfs.h"

#include <map>
#include <string>

typedef struct localfs_check_path
{
    const char* root_path;
    const char* search_item;
} localfs_check_path_t;

typedef std::map<std::string, vfs_stat_t> ItemMap;

TEST_FIXTURE_SETUP(localfs)
{
    ASSERT_EQ_INT(0, vfs_init());
}

TEST_FIXTURE_TEARDOWN(localfs)
{
    vfs_exit();
}

static int _localfs_test_ls_cb(const char* name, const vfs_stat_t* stat, void* data)
{
    ItemMap* items = static_cast<ItemMap*>(data);
    items->insert(ItemMap::value_type(name, *stat));
    return 0;
}

TEST_F(localfs, ls_root)
{
    {
        vfs_operations_t* fs = NULL;
        ASSERT_EQ_INT(vfs_make_local(&fs, "/"), 0);
        ASSERT_NE_PTR(fs, NULL);
        ASSERT_EQ_INT(vfs_mount("/", fs), 0);
    }

    vfs_operations_t* vfs = vfs_visitor_instance();
    ASSERT_NE_PTR(vfs, NULL);

    ItemMap items;
    ASSERT_EQ_INT(vfs->ls(vfs, "/", _localfs_test_ls_cb, &items), 0);

    ASSERT_GT_SIZE(items.size(), 0);

#if defined(_WIN32)
    const char* match = "C:";
#else
    const char* match = "dev";
#endif

    ASSERT_EQ_INT(items.find(match) != items.end(), 1);
}

#if defined(_WIN32)
#define TEST_CHECK_PATH     \
    { "C:", "Windows" },    \
    { "C:/", "Windows" },   \
    { "C:\\", "Windows" }
#else
#define TEST_CHECK_PATH     \
    { "/dev", "null" },     \
    { "/dev/", "null" }
#endif

TEST_PARAMETERIZED_DEFINE(localfs, ls_mount, localfs_check_path_t, TEST_CHECK_PATH);

TEST_P(localfs, ls_mount)
{
    /*
     * It does not matter we are mounted. The path is always converted by VFS.
     */
    const char* mount_path = "file:///";

    localfs_check_path_t path = TEST_GET_PARAM();

    {
        vfs_operations_t* fs = NULL;
        ASSERT_EQ_INT(vfs_make_local(&fs, path.root_path), 0);
        ASSERT_NE_PTR(fs, NULL);
        ASSERT_EQ_INT(vfs_mount(mount_path, fs), 0);
    }

    vfs_operations_t* vfs = vfs_visitor_instance();
    ASSERT_NE_PTR(vfs, NULL);

    {
        ItemMap items;
        vfs->ls(vfs, mount_path, _localfs_test_ls_cb, &items);
        ASSERT_EQ_INT(items.find(path.search_item) != items.end(), 1);
    }
}
