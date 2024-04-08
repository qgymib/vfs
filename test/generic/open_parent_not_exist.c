#include "__init__.h"

static void _vfs_test_generic_open_parent_not_exist(vfs_operations_t* fs)
{
    uintptr_t fh;
    ASSERT_EQ_INT(fs->open(fs, &fh, "/foo/bar", VFS_O_WRONLY | VFS_O_CREATE), VFS_ENOENT);
}

/**
 * @brief create file whose parent is not exist.
 */
const vfs_test_generic_case_t vfs_test_generic_open_parent_not_exist = {
    "open_parent_not_exist", _vfs_test_generic_open_parent_not_exist,
};
