#include "__init__.h"

static void _vfs_test_generic_rmdir_type_mismatch(vfs_operations_t* fs)
{
    uintptr_t fh;
    const char* path = "/rmdir_mismatch";
    ASSERT_EQ_INT(fs->open(fs, &fh, path, VFS_O_WRONLY | VFS_O_CREATE), 0);
    ASSERT_EQ_INT(fs->close(fs, fh), 0);

    vfs_stat_t info;
    ASSERT_EQ_INT(fs->stat(fs, path, &info), 0);
    ASSERT_EQ_UINT64(info.st_mode, VFS_S_IFREG);

    /* try to rmdir a file. */
    ASSERT_EQ_INT(fs->rmdir(fs, path), VFS_ENOTDIR);

    ASSERT_EQ_INT(fs->unlink(fs, path), 0);
    ASSERT_EQ_INT(fs->stat(fs, path, &info), VFS_ENOENT);
}

const vfs_test_generic_case_t vfs_test_generic_rmdir_type_mismatch = {
    "rmdir_type_mismatch", _vfs_test_generic_rmdir_type_mismatch,
};
