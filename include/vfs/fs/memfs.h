#ifndef __VFS_MEMORY_FS_H__
#define __VFS_MEMORY_FS_H__

#include "vfs/vfs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a in-memory file system
 * @param[out] fs - The created file system.
 * @return - 0: on success.
 * @return - -errno: on failure.
 */
int vfs_make_memory(vfs_operations_t** fs);

#ifdef __cplusplus
}
#endif
#endif
