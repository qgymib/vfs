#ifndef __VFS_TEST_GENERIC_FS_TESTER_H__
#define __VFS_TEST_GENERIC_FS_TESTER_H__

#include "vfs/vfs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Generic file system tester.
 * @param[in] fs - The file system.
 */
void vfs_test_generic(vfs_operations_t* fs);

#ifdef __cplusplus
}
#endif
#endif
