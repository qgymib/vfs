#include "__init__.h"

static void _test_generic_check_root(vfs_operations_t* fs)
{
	vfs_stat_t info;
	ASSERT_EQ_INT(fs->stat(fs, "/", &info), 0);
	ASSERT_EQ_UINT64(info.st_mode, VFS_S_IFDIR);
}

/**
 * @brief Check the root stat of file system.
 */
const vfs_test_generic_case_t vfs_test_generic_check_root = {
	"check_root", _test_generic_check_root,
};
