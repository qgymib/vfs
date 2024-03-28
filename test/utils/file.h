#ifndef __VFS_TEST_FILE_H__
#define __VFS_TEST_FILE_H__

#include "utils/str.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Load the content of \p path to \p dat.
 * @param[out] dat - The buffer to hold file content.
 * @param[in] path - The path of the file.
 */
void vfs_test_read_file(vfs_str_t* dat, const char* path);

#ifdef __cplusplus
}
#endif
#endif
