#include "__init__.h"

static void _vfs_test_generic_mkdir_rmdir_in_root(vfs_operations_t* fs)
{
    vfs_stat_t info;
    const char* path = "/generic_fs_tester";

    /* mkdir */
    ASSERT_EQ_INT(fs->mkdir(fs, path), 0);
    ASSERT_EQ_INT(fs->stat(fs, path, &info), 0);
    ASSERT_EQ_UINT64(info.st_mode, VFS_S_IFDIR);

    /* rmdir */
    ASSERT_EQ_INT(fs->rmdir(fs, path), 0);
    ASSERT_EQ_INT(fs->stat(fs, path, &info), VFS_ENOENT);
}

const vfs_test_generic_case_t vfs_test_generic_mkdir_rmdir_in_root = {
    "mkdir_rmdir_in_root", _vfs_test_generic_mkdir_rmdir_in_root,
};
