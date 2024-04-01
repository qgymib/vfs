#ifndef __VFS_OVERLAY_FS_H__
#define __VFS_OVERLAY_FS_H__

#include "vfs/vfs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create an overlay file system.
 * @param[out] fs - The created file system.
 * @param[in] lower - The lower file system.
 * @param[in] upper - The upper file system.
 * @return - 0: on success.
 * @return - -errno: on error.
*/
int vfs_make_overlay(vfs_operations_t** fs, vfs_operations_t* lower,
	vfs_operations_t* upper);

#ifdef __cplusplus
}
#endif
#endif
