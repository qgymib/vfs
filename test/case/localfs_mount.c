#include "test.h"
#include "vfs/fs/localfs.h"

TEST_FIXTURE_SETUP(localfs)
{
    ASSERT_EQ_INT(0, vfs_init());
}

TEST_FIXTURE_TEARDOWN(localfs)
{
    vfs_exit();
}

TEST_F(localfs, mount_non_exist)
{
    vfs_str_t tmp_path = vfs_str_dup(&g_cwd_path);
    vfs_str_append1(&tmp_path, "/test_non_exist_dir");

    vfs_operations_t* fs;
    ASSERT_EQ_INT(vfs_make_local(&fs, tmp_path.str), VFS_ENOENT);

    vfs_str_exit(&tmp_path);
}
