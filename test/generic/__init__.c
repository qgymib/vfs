#include "utils/defs.h"
#include "__init__.h"

static const vfs_test_generic_case_t* s_test_generic_cases[] = {
    &vfs_test_generic_check_root,
    &vfs_test_generic_mkdir_parent_not_exist,
    &vfs_test_generic_mkdir_rmdir_in_root,
    &vfs_test_generic_open_as_rdonly_and_write,
    &vfs_test_generic_open_as_wronly_and_read,
    &vfs_test_generic_open_parent_not_exist,
    &vfs_test_generic_open_unlink_in_root,
    &vfs_test_generic_rmdir_non_empty,
    &vfs_test_generic_rmdir_type_mismatch,
    &vfs_test_generic_truncate_larget_and_seek,
    &vfs_test_generic_truncate_smaller_and_seek,
    &vfs_test_generic_unlink_type_mismatch,
};

static int _vfs_test_generic_count_cb(const char* name, const vfs_stat_t* stat, void* data)
{
    (void)name; (void)stat;

    size_t* cnt = data;
    *cnt += 1;

    return 0;
}

static size_t _vfs_test_generic_count_item(vfs_operations_t* fs, const char* path)
{
    size_t cnt = 0;
    ASSERT_EQ_INT(fs->ls(fs, path, _vfs_test_generic_count_cb, &cnt), 0);
    return cnt;
}

void vfs_test_generic(vfs_operations_t* fs)
{
    size_t i;
    size_t root_sz = _vfs_test_generic_count_item(fs, "/");

    for (i = 0; i < ARRAY_SIZE(s_test_generic_cases); i++)
    {
        const vfs_test_generic_case_t* test_case = s_test_generic_cases[i];
        test_case->entry(fs);
    }

    /* After all test, the root state should remain the same. */
    ASSERT_EQ_SIZE(_vfs_test_generic_count_item(fs, "/"), root_sz);
}
