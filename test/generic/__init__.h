#ifndef __VFS_TEST_GENERIC_INIT_H__
#define __VFS_TEST_GENERIC_INIT_H__

#include "vfs/vfs.h"
#include "test.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vfs_test_generic_case
{
    const char* name;   /**< The name of the test case. */

    /**
     * @brief Test case entry point.
     * @param[in] fs - The file system.
     */
    void (*entry)(vfs_operations_t* fs);
} vfs_test_generic_case_t;

extern const vfs_test_generic_case_t vfs_test_generic_check_root;
extern const vfs_test_generic_case_t vfs_test_generic_mkdir_parent_not_exist;
extern const vfs_test_generic_case_t vfs_test_generic_mkdir_rmdir_in_root;
extern const vfs_test_generic_case_t vfs_test_generic_open_parent_not_exist;
extern const vfs_test_generic_case_t vfs_test_generic_open_unlink_in_root;
extern const vfs_test_generic_case_t vfs_test_generic_rmdir_non_empty;
extern const vfs_test_generic_case_t vfs_test_generic_rmdir_type_mismatch;
extern const vfs_test_generic_case_t vfs_test_generic_truncate_larget_and_seek;
extern const vfs_test_generic_case_t vfs_test_generic_truncate_smaller_and_seek;
extern const vfs_test_generic_case_t vfs_test_generic_unlink_type_mismatch;

/**
 * @brief Generic file system tester.
 * @param[in] fs - The file system.
 */
void vfs_test_generic(vfs_operations_t* fs);

#ifdef __cplusplus
}
#endif
#endif
