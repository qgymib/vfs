#ifndef __VFS_TEST_FILE_H__
#define __VFS_TEST_FILE_H__

#include "utils/str.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Load the content of \p path to \p dat.
 * @param[in] fs - The file system.
 * @param[in] path - The path of the file.
 * @param[out] dat - The buffer to hold file content.
 */
void vfs_test_read_file(vfs_operations_t* fs, const char* path, vfs_str_t* dat);

/**
 * @brief Write \p data to \p path.
 * @param[in] fs - The file system.
 * @param[in] path - The path of the file.
 * @param[in] data - The data to write.
 * @param[in] size - The size of \p data.
*/
void vfs_test_write_file(vfs_operations_t* fs, const char* path, const void* data, size_t size);

#ifdef __cplusplus
}
#endif
#endif
