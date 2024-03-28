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
static vfs_mount_t* _vfs_fetch_path_and_add_node(const vfs_str_t* path)
{
    vfs_mount_t tmp_node;
    tmp_node.path = *path;

    vfs_mount_t* node = NULL;
    vfs_rwlock_rdlock(&g_vfs->mount_map_lock);
    do {
        ev_map_node_t* it = vfs_map_find_upper(&g_vfs->mount_map, &tmp_node.node);
        if (it != NULL)
        {
            it = vfs_map_prev(it);
        }
        else
        {
            it = vfs_map_end(&g_vfs->mount_map);
        }

        if (it == NULL)
        {
            break;
        }

        node = EV_CONTAINER_OF(it, vfs_mount_t, node);
        if (vfs_str_startwith2(path, &node->path))
        {
            (void)vfs_atomic_add(&node->refcnt);
            break;
        }

        node = NULL;
    } while (0);
    vfs_rwlock_rdunlock(&g_vfs->mount_map_lock);

    return node;
}

static vfs_str_t _vfs_get_relative_path(const vfs_mount_t* node, const vfs_str_t* path)
{
    if (node->path.len == path->len)
    {
        return vfs_str_from_static1("/");
    }

    return vfs_str_sub(path, node->path.len, (size_t)-1);
}

int vfs_access_mount(const vfs_str_t* path, vfs_path_cb cb, void* data)
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

    vfs_str_t relative_path = _vfs_get_relative_path(node, path);
    ret = cb(node, &relative_path, data);
    vfs_str_exit(&relative_path);

finish:
    vfs_release_mount(node);
    return ret;
}

void vfs_release_mount(vfs_mount_t* point)
{
    if (vfs_atomic_dec(&point->refcnt) != 0)
    {
        return;
    }

    if (point->op != NULL)
    {
        point->op->destroy(point->op);
        point->op = NULL;
    }

    vfs_str_exit(&point->path);
    free(point);
}
