#ifndef __VFS_LOCAL_FS_H__
#define __VFS_LOCAL_FS_H__

#include "vfs/vfs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create local file system.
 * @param[out] fs - File system instance.
 * @param[in] root - The root path.
 * @return 0 on success, or -errno on failure.
 */
int vfs_make_local(vfs_operations_t** fs, const char* root);

#ifdef __cplusplus
}
#endif
#endif
