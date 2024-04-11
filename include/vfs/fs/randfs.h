#ifndef __VFS_RANDOM_FS_H__
#define __VFS_RANDOM_FS_H__

#include "vfs/vfs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a random file system.
 * @param[out] fs - The created file system.
 * @return - 0: on success.
 * @return - -errno: on failure.
 */
int vfs_make_random(vfs_operations_t** fs);

#ifdef __cplusplus
}
#endif
#endif
