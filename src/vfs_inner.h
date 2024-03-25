#ifndef __VFS_INNER_H__
#define __VFS_INNER_H__

#include "vfs.h"
#include "utils/atomic.h"
#include "utils/map.h"
#include "utils/rwlock.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Virtual file system mount point.
*/
typedef struct vfs_mount_s
{
    ev_map_node_t       node;           /**< Node in map. See #vfs_ctx_t::mount_map. */

    vfs_atomic_t        refcnt;         /**< Reference count. */
    char*               path;           /**< Mount path. */
    size_t              path_sz;        /**< Mount path size. */

    vfs_operations_t*   op;             /**< Filesystem operations. */
} vfs_mount_t;

typedef struct vfs_ctx
{
    ev_map_t            mount_map;          /**< Mount map. See #vfs_mount_t. */
    vfs_rwlock_t        mount_map_lock;     /**< Mount map lock. */
    vfs_operations_t*   visitor;            /**< Visitor file system. */
} vfs_ctx_t;

extern vfs_ctx_t*       g_vfs;

/**
 * @brief Operation protected callback.
 * @param[in] fs - File system node.
 * @param[in] path - Formatted path.
 * @param[in] data - User defined data.
 * @return Any value that return to caller.
 */
typedef int (*vfs_path_cb)(vfs_mount_t* fs, const char* path, void* data);

/**
 * @brief Perform safe operation on \p path.
 * @param[in] path - The path to access.
 * @param[in] cb - Operation callback.
 * @param[in] data - User defined data.
 * @return 0 if success, or -errno if failure.
 */
int vfs_op_safe(const char* path, vfs_path_cb cb, void* data);

/**
 * @brief Decrement reference count of \p node.
 * 
 * To add reference count, use #vfs_atomic_add() directly on #vfs_mount_t::refcnt.
 *
 * @param[in] node - File system node.
 */
void vfs_dec_node(vfs_mount_t* node);

#ifdef __cplusplus
}
#endif
#endif
