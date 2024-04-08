#include "__init__.h"

static void _vfs_test_generic_mkdir_parent_not_exist(vfs_operations_t* fs)
{
    const char* path = "/hello/" __DATE__;
    /* create directory whose parent is not exist. */
    ASSERT_EQ_INT(fs->mkdir(fs, path), VFS_ENOENT);
}

const vfs_test_generic_case_t vfs_test_generic_mkdir_parent_not_exist = {
    "mkdir_parent_not_exist", _vfs_test_generic_mkdir_parent_not_exist,
};
