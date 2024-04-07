#ifndef __VFS_TEST_FILE_SYSTEM_BUILDER_H__
#define __VFS_TEST_FILE_SYSTEM_BUILDER_H__

#include "vfs/utils/dir.h"
#include "vfs/utils/file.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vfs_fsbuilder_item
{
    const char*     name;           /**< File name. */
    vfs_stat_flag_t type;           /**< File type. */
} vfs_fsbuilder_item_t;

/**
 * @brief Build a file system.
 * @param[in] fs - The file system.
 * @param[in] items - File system items to build.
 * @param[in] prefix - (Optional) The prefix of the file system.
 */
void vfs_test_fs_build(vfs_operations_t* fs, const vfs_fsbuilder_item_t* items,
    const char* prefix);

/**
 * @brief Load the content of \p path to \p dat.
 * @param[in] fs - The file system.
 * @param[in] path - The path of the file.
 * @param[out] dat - The buffer to hold file content.
 */
void vfs_test_read_file(vfs_operations_t* fs, const char* path, vfs_str_t* dat);

#ifdef __cplusplus
}
#endif
#endif
