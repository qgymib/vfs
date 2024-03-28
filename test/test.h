#ifndef __VFS_TEST_H__
#define __VFS_TEST_H__

#include "cutest.h"
#include "utils/str.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Native path to the executable.
 */
extern vfs_str_t g_exe_path;

/**
 * @brief Native path to the current working directory.
 */
extern vfs_str_t g_cwd_path;

/**
 * @brief Initialize the test suite.
 * @param[in] argc - The number of arguments
 * @param[in] argv - Argument list
 */
void vfs_test_init(int argc, char* argv[]);

/**
 * @brief Exit the test suite.
 */
void vfs_test_exit(void);

#ifdef __cplusplus
}
#endif
#endif
