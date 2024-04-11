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

/**
 * @brief Generic file system tester.
 * @param[in] fs - The file system.
 */
void vfs_test_generic(vfs_operations_t* fs);

#ifdef __cplusplus
}
#endif
#endif
