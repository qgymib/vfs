#ifndef __VFS_MEMFS_INNER_H__
#define __VFS_MEMFS_INNER_H__

#include "vfs/fs/memfs.h"
#include "utils/atomic.h"
#include "utils/map.h"
#include "utils/mutex.h"
#include "utils/rwlock.h"
#include "utils/str.h"

#ifdef __cplusplus
extern "C" {
#endif

struct vfs_memfs_node;

typedef struct vfs_memfs_node_dir
{
    struct vfs_memfs_node**     children;           /**< This node's children. */
    size_t                      children_sz;        /**< The number of children. */
    size_t                      children_cap;       /**< The capacity of children. */
} vfs_memfs_node_dir_t;

typedef struct vfs_memfs_node_reg
{
    uint8_t*                    data;               /**< File content. '\0' terminal is always appended. */
} vfs_memfs_node_reg_t;

typedef struct vfs_memfs_node
{
    vfs_atomic_t                refcnt;             /**< Reference count. */
    vfs_rwlock_t                rwlock;             /**< RW lock for everything except refcnt. */
    vfs_str_t                   name;               /**< The name of this node. */
    vfs_stat_t                  stat;               /**< The stat of this node. */
    struct vfs_memfs_node*      parent;             /**< This node's parent. */

    union
    {
        vfs_memfs_node_dir_t    dir;                /**< Directory, if #vfs_memfs_node_t::stat::st_mode contains #VFS_S_IFDIR. */
        vfs_memfs_node_reg_t    reg;                /**< Regular file, if #vfs_memfs_node_t::stat::st_mode contains #VFS_S_IFREG. */
    } data;
} vfs_memfs_node_t;

typedef struct vfs_memfs_session
{
    ev_map_node_t               node;               /**< Session map node. */
    vfs_atomic_t                refcnt;             /**< Reference count. */
    vfs_mutex_t                 mutex;              /**< Mutex for this session. */

    struct
    {
        uintptr_t               fh;                 /**< File handle. */
        uint64_t                flags;              /**< Open flags. */
        uint64_t                fpos;               /**< File position. value of #UINT64_MAX means always append to end. */
        vfs_memfs_node_t*       node;               /**< File system node with reference count increased. */
    } data;
} vfs_memfs_session_t;

/**
 * @brief Memory File system IO layer.
 */
typedef struct vfs_memfs_io
{
    void* data;

    /**
     * @brief Read callback.
     * @param[in] session - The session.
     * @param[in] buf - The buffer.
     * @param[in] len - The length.
     * @param[in] data - The callback data.
     * @return The number of bytes read on success, or -errno on error.
     */
    int (*read)(vfs_memfs_session_t* session, void* buf, size_t len, void* data);

    /**
     * @brief Write callback.
     * @param[in] session - The session.
     * @param[in] buf - The buffer.
     * @param[in] len - The length.
     * @param[in] data - The callback data.
     * @return The number of bytes written on success, or -errno on error.
     */
	int (*write)(vfs_memfs_session_t* session, const void* buf, size_t len, void* data);
} vfs_memfs_io_t;

/**
 * @brief Set IO layer.
 * @param[in,out] fs - File system.
 * @param[in] io - IO layer.
 */
void vfs_memfs_set_io(vfs_operations_t* fs, const vfs_memfs_io_t* io);

#ifdef __cplusplus
}
#endif
#endif
