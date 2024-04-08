#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "utils/defs.h"
#include "vfs_inner.h"
#include "vfs_visitor.h"

vfs_ctx_t* g_vfs = NULL;

static int _vfs_map_cmp_mount(const ev_map_node_t* key1, const ev_map_node_t* key2, void* arg)
{
    (void)arg;
    vfs_mount_t* node1 = EV_CONTAINER_OF(key1, vfs_mount_t, node);
    vfs_mount_t* node2 = EV_CONTAINER_OF(key2, vfs_mount_t, node);
    return strcmp(node1->path.str, node2->path.str);
}

static void _vfs_setup_node(vfs_mount_t* node, const char* path, vfs_operations_t* op)
{
    node->refcnt = 1;
    node->path = vfs_str_from1(path);
    node->op = op;

    /*
     * Format path to remove trailing '/'.
     * VFS convert following patterns:
     * 1. "/" --> "/"
     * 2. "/foo" --> "/foo"
     * 3. "/foo/" --> "/foo"
     * 4. "file:///" --> "file:///"
     * 5. "file:///foo" --> "file:///foo"
     * 6. "file:///foo/" --> "file:///foo"
     */
    if (node->path.len >= 2 && node->path.str[node->path.len - 1] == '/' && node->path.str[node->path.len - 2] != '/')
    {
        vfs_str_chop(&node->path, 1);
    }
}

static void _vfs_cleanup_mount(void)
{
    ev_map_node_t* it;

    vfs_rwlock_wrlock(&g_vfs->mount_map_lock);
    while ((it = vfs_map_begin(&g_vfs->mount_map)) != NULL)
    {
        vfs_mount_t* node = EV_CONTAINER_OF(it, vfs_mount_t, node);
        vfs_map_erase(&g_vfs->mount_map, it);

        vfs_rwlock_wrunlock(&g_vfs->mount_map_lock);
        {
            vfs_release_mount(node);
        }
        vfs_rwlock_wrlock(&g_vfs->mount_map_lock);
    }
    vfs_rwlock_wrunlock(&g_vfs->mount_map_lock);
}

int vfs_init(void)
{
    if (g_vfs != NULL)
    {
        return VFS_EALREADY;
    }

    if ((g_vfs = calloc(1, sizeof(vfs_ctx_t))) == NULL)
    {
        return VFS_ENOMEM;
    }
    
    vfs_map_init(&g_vfs->mount_map, _vfs_map_cmp_mount, NULL);
    vfs_rwlock_init(&g_vfs->mount_map_lock);
    g_vfs->visitor = vfs_create_visitor();

    return 0;
}

void vfs_exit(void)
{
    if (g_vfs == NULL)
    {
        return;
    }

    if (g_vfs->visitor != NULL)
    {
        g_vfs->visitor->destroy(g_vfs->visitor);
        g_vfs->visitor = NULL;
    }
    
    _vfs_cleanup_mount();
    vfs_rwlock_exit(&g_vfs->mount_map_lock);

    free(g_vfs);
    g_vfs = NULL;
}

int vfs_mount(const char* path, vfs_operations_t* op)
{
    if (path == NULL || op == NULL)
    {
        return VFS_EINVAL;
    }

    vfs_mount_t* node = malloc(sizeof(vfs_mount_t));
    if (node == NULL)
    {
        return -ENOMEM;
    }
    _vfs_setup_node(node, path, op);

    ev_map_node_t* orig;
    vfs_rwlock_wrlock(&g_vfs->mount_map_lock);
    {
        orig = vfs_map_insert(&g_vfs->mount_map, &node->node);
    }
    vfs_rwlock_wrunlock(&g_vfs->mount_map_lock);

    if (orig != NULL)
    {
        free(node);
        return VFS_EALREADY;
    }

    return 0;
}

int vfs_unmount(const char* path)
{
    vfs_mount_t tmp_node;
    tmp_node.path = vfs_str_from_static1(path);

    vfs_mount_t* node = NULL;
    vfs_rwlock_wrlock(&g_vfs->mount_map_lock);
    {
        ev_map_node_t*  it = vfs_map_find(&g_vfs->mount_map, &tmp_node.node);
        if (it != NULL)
        {
            node = EV_CONTAINER_OF(it, vfs_mount_t, node);
            vfs_map_erase(&g_vfs->mount_map, it);
        }
    }
    vfs_rwlock_wrunlock(&g_vfs->mount_map_lock);

    vfs_str_exit(&tmp_node.path);
    if (node == NULL)
    {
        return VFS_ENOENT;
    }

    vfs_release_mount(node);

    return 0;
}

vfs_operations_t* vfs_visitor_instance(void)
{
    return g_vfs->visitor;
}
