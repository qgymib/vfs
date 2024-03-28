#ifndef __VFS_TEST_CWD_PATH_H__
#define __VFS_TEST_CWD_PATH_H__

#include "utils/str.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get the current working directory.
 * @return The current working directory.
 */
vfs_str_t vfs_test_cwd_path(void);

#ifdef __cplusplus
}
#endif
#endif
