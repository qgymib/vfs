#include "utils/dir.h"
#include "utils/file.h"
#include "__init__.h"

static void _vfs_test_generic_rmdir_non_empty(vfs_operations_t* fs)
{
    {
        ASSERT_EQ_INT(vfs_dir_make(fs, "/foo/bar"), 0);
        ASSERT_EQ_INT(fs->rmdir(fs, "/foo"), VFS_ENOTEMPTY);
        ASSERT_EQ_INT(vfs_dir_delete(fs, "/foo"), 0);
    }
    {
        uintptr_t fh;
        ASSERT_EQ_INT(vfs_file_open(fs, &fh, "/hello/world", VFS_O_WRONLY | VFS_O_CREATE), 0);
        ASSERT_EQ_INT(fs->close(fs, fh), 0);
        ASSERT_EQ_INT(fs->rmdir(fs, "/hello"), VFS_ENOTEMPTY);
        ASSERT_EQ_INT(vfs_dir_delete(fs, "/hello"), 0);
    }
}

/**
 * @brief rmdir non-empty directory.
 */
const vfs_test_generic_case_t vfs_test_generic_rmdir_non_empty = {
    "rmdir_non_empty", _vfs_test_generic_rmdir_non_empty,
};
