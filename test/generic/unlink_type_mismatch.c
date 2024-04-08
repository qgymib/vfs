#include "__init__.h"

static void _vfs_test_generic_unlink_type_mismatch(vfs_operations_t* fs)
{
    const char* path = "/unlink_mismatch";

    ASSERT_EQ_INT(fs->mkdir(fs, path), 0);

    vfs_stat_t info;
    ASSERT_EQ_INT(fs->stat(fs, path, &info), 0);
    ASSERT_EQ_UINT64(info.st_mode, VFS_S_IFDIR);

    /* try to unlink a directory. */
    ASSERT_EQ_INT(fs->unlink(fs, path), VFS_EISDIR);

    ASSERT_EQ_INT(fs->rmdir(fs, path), 0);
    ASSERT_EQ_INT(fs->stat(fs, path, &info), VFS_ENOENT);
}

const vfs_test_generic_case_t vfs_test_generic_unlink_type_mismatch = {
    "unlink_type_mismatch", _vfs_test_generic_unlink_type_mismatch,
};
