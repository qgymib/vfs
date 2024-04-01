#ifndef __VFS_TEST_OVERLAY_FS_BUILDER_H__
#define __VFS_TEST_OVERLAY_FS_BUILDER_H__

#include "vfs/fs/localfs.h"
#include "vfs/fs/overlayfs.h"
#include "utils/str.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vfs_fsbuilder_item
{
    const char*     name;           /**< File name. */
    vfs_stat_flag_t type;           /**< File type. */
} vfs_fsbuilder_item_t;

typedef struct overlayfs_quick
{
    vfs_operations_t*   fs_overlay; /**< The overlay file system. */

    vfs_str_t           lower_path;
    vfs_operations_t*   fs_lower;   /**< The lower read-only file system. */

    vfs_str_t           upper_path;
    vfs_operations_t*   fs_upper;   /**< The upper read-write file system. */

    vfs_str_t           cwd_path;   /**< The current working directory. */
    vfs_operations_t*   fs_local;   /**< The local read-write file system works on '/'. */

    vfs_str_t           mount;      /**< The mount point. */
} overlayfs_quick_t;

/**
 * @brief Create an overlay file system builder.
 * @param[in] mount - The mount point.
 * @param[in] lower - The lower layer items, the last item must be NULL,
 * @param[in] upper - The upper layer items, the last item must be NULL,
 * @return - The created builder.
 */
overlayfs_quick_t* vfs_overlayfs_quick_make(const char* mount,
    const vfs_fsbuilder_item_t* lower, const vfs_fsbuilder_item_t* upper);

/**
 * @brief Exit the overlay file system builder.
 * @param[in,out] quick - The builder.
 */
void vfs_overlayfs_quick_exit(overlayfs_quick_t* quick);

#ifdef __cplusplus
}
#endif
#endif
