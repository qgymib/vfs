#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "utils/defs.h"
#include "vfs_inner.h"

/**
 * @brief Search for mount point for \p path. If found, add reference count and return it.
 * @param[in] path - Path to the filesystem. Encoding in UTF-8.
 * @return vfs_mount_t* on success, or NULL on error.
*/
static vfs_mount_t* _vfs_fetch_path_and_add_node(const char* path)
{
    vfs_mount_t tmp_node;
    tmp_node.path = (char*)path;

    vfs_mount_t* node = NULL;
    vfs_rwlock_rdlock(&g_vfs->mount_map_lock);
    do {
        ev_map_node_t* it = vfs_map_find_upper(&g_vfs->mount_map, &tmp_node.node);
        if (it == NULL)
        {
            it = vfs_map_end(&g_vfs->mount_map);
        }

        it = vfs_map_prev(it);
        if (it == NULL)
        {
            break;
        }

        node = EV_CONTAINER_OF(it, vfs_mount_t, node);
        if (strncmp(path, node->path, node->path_sz) == 0)
        {
            (void)vfs_atomic_add(&node->refcnt);
            break;
        }

        node = NULL;
    } while (0);
    vfs_rwlock_rdunlock(&g_vfs->mount_map_lock);

    return node;
}

static const char* _vfs_get_relative_path(const vfs_mount_t* node, const char* path)
{
    return path + node->path_sz;
}

int vfs_op_safe(const char* path, vfs_path_cb cb, void* data)
{
    vfs_mount_t* node = _vfs_fetch_path_and_add_node(path);
    if (node == NULL)
    {
        return -ENOENT;
    }

    int ret;
    if (node->op == NULL)
    {
        ret = -ENOSYS;
        goto finish;
    }

    const char* relative_path = _vfs_get_relative_path(node, path);
    ret = cb(node, relative_path, data);

finish:
    vfs_dec_node(node);
    return ret;
}

void vfs_dec_node(vfs_mount_t* node)
{
    if (vfs_atomic_dec(&node->refcnt) != 0)
    {
        return;
    }

    if (node->op != NULL)
    {
        node->op->destroy(node->op);
        node->op = NULL;
    }

    free(node);
}
