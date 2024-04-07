#ifndef __VFS_DIR_H__
#define __VFS_DIR_H__

#include "vfs/vfs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create directory.
 *
 * Like #vfs_operations_t::mkdir(), this function create directory \p path, with
 * exception that:
 * 1. If parent directory does not exist, it will be created.
 *
 * @param[in] fs - The file system.
 * @param[in] path - The path of the directory, encoding in UTF-8.
 * @return - 0: on success.
 * @return - -errno: on error.
 */
int vfs_dir_make(vfs_operations_t* fs, const char* path);

/**
 * @brief Delete directory.
 *
 * Like #vfs_operations_t::rmdir(), this function delete directory \p path, with
 * exception that:
 * 1. If directory is not empty, all contents in it will be deleted.
 *
 * @param[in] fs - The file system.
 * @param[in] path - The path of the directory, encoding in UTF-8.
 * @return - 0: on success.
 * @return - #VFS_ENOSYS: \p fs does not implement #vfs_operations_t::rmdir(),
 *   #vfs_operations_t::unlink() or #vfs_operations_t::ls().
 * @return - -errno: on error.
 */
int vfs_dir_delete(vfs_operations_t* fs, const char* path);

#ifdef __cplusplus
}
#endif
#endif
