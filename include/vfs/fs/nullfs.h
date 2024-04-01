#ifndef __VFS_NULL_FS_H__
#define __VFS_NULL_FS_H__

#include "vfs/vfs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a NULL file system, which behaves like `/dev/null`.
 *
 * It can handle regular file operations like mkdir/rmdir/ln but writing to
 * files does not store any data. The file size is however saved, reading from
 * the files behaves like reading from `/dev/zero`.
 *
 * @param[out] fs - The created file system.
 * @return - 0: on success.
 * @return - -errno: on error.
 */
int vfs_make_null(vfs_operations_t** fs);

#ifdef __cplusplus
}
#endif
#endif
