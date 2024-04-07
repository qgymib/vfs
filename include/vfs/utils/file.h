#ifndef __VFS_FILE_H__
#define __VFS_FILE_H__

#include "vfs/vfs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Open file.
 *
 * Like #vfs_operations_t::open(), this function open file \p path with \p flags,
 * with exception that:
 * 1. if \p flags contains #VFS_O_CREAT, the file and parent directory will be
 *   created if it does not exist.
 *
 * @see #vfs_open_flag_t.
 * @see #vfs_operations_t::open().
 * @see #vfs_operations_t::close().
 * @param[in,out] fs - The file system.
 * @param[out] fh - The file handle. Use #vfs_file_close() to close it.
 * @param[in] path - The path of the file, encoding in UTF-8.
 * @param[in] flags - The open flags. See #vfs_open_flag_t.
 * @return - 0: on success.
 * @return - #VFS_EINVAL: \p path is invalid.
 * @return - -errno: on error.
 */
int vfs_file_open(vfs_operations_t* fs, uintptr_t* fh, const char* path, uint64_t flags);

/**
 * @brief Open file using #vfs_file_open(), then write data to it, and close the
 *   file handle.
 * @see vfs_file_open().
 * @param[in,out] fs - The file system.
 * @param[in] path - The path of the file, encoding in UTF-8.
 * @param[in] flags - The open flags. See #vfs_open_flag_t.
 * @param[in] buf - The data to write.
 * @param[in] len - The size of \p buf.
 * @return - 0: on success.
 * @return - #VFS_ENOSYS: Missing implementation of #vfs_operations_t::write()
 *   or #vfs_operations_t::close().
 * @return - -errno: on error.
 */
int vfs_file_write(vfs_operations_t* fs, const char* path, uint64_t flags,
    const void* buf, size_t len);

#ifdef __cplusplus
}
#endif
#endif
