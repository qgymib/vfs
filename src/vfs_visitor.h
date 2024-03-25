#ifndef __VFS_VISITOR_H__
#define __VFS_VISITOR_H__

#include "vfs_inner.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vfs_session
{
    ev_map_node_t       node;           /**< Node in map. See #vfs_ctx_t::session_map. */

    vfs_atomic_t        refcnt;         /**< Reference count. */
    uint64_t            fake;           /**< Fake file handle. */
    uint64_t            real;           /**< Real file handle. */

    /**
     * @brief Mounted node.
     * It handle reference to the node, and must decrease when released.
     */
    vfs_mount_t*        mount;
} vfs_session_t;

typedef struct vfs_visitor_s
{
    vfs_operations_t    op;                 /**< File system operations. */
    vfs_atomic64_t      fh_gen;             /**< File handle generator. */
    ev_map_t            session_map;        /**< Session map. See #vfs_session_t. */
    vfs_rwlock_t        session_map_lock;   /**< Session map lock. */
} vfs_visitor_t;

/**
 * @brief Create visitor file system.
 * @return Visitor file system.
 */
vfs_operations_t* vfs_create_visitor(void);

#ifdef __cplusplus
}
#endif
#endif
