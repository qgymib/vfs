#ifndef __VFS_TEST_EXE_PATH_H__
#define __VFS_TEST_EXE_PATH_H__

#include "utils/str.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get the executable path.
 * @return The executable path.
 */
vfs_str_t vfs_test_exe_path(void);

#ifdef __cplusplus
}
#endif
#endif
