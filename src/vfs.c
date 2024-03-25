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
    return strcmp(node1->path, node2->path);
}

static void _vfs_setup_node(vfs_mount_t* node, const char* path, size_t path_sz, vfs_operations_t* op)
{
    node->refcnt = 1;
    node->path = (char*)(node + 1);
    memcpy(node->path, path, path_sz + 1);
    node->path_sz = path_sz;
    node->op = op;

    /* Remove trailing '/' */
    if (node->path[node->path_sz] == '/')
    {
        node->path[node->path_sz] = '\0';
        node->path_sz--;
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
            vfs_dec_node(node);
        }
        vfs_rwlock_wrlock(&g_vfs->mount_map_lock);
    }
    vfs_rwlock_wrunlock(&g_vfs->mount_map_lock);
}

int vfs_init(void)
{
    if (g_vfs != NULL)
    {
        return -EALREADY;
    }

    if ((g_vfs = calloc(1, sizeof(vfs_ctx_t))) == NULL)
    {
        return -ENOMEM;
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
    size_t path_sz = strlen(path);
    size_t malloc_sz = sizeof(vfs_mount_t) + path_sz + 1;

    vfs_mount_t* node = calloc(1, malloc_sz);
    if (node == NULL)
    {
        return -ENOMEM;
    }
    _vfs_setup_node(node, path, path_sz, op);

    ev_map_node_t* orig;
    vfs_rwlock_wrlock(&g_vfs->mount_map_lock);
    {
        orig = vfs_map_insert(&g_vfs->mount_map, &node->node);
    }
    vfs_rwlock_wrunlock(&g_vfs->mount_map_lock);

    if (orig != NULL)
    {
        free(node);
        return -EALREADY;
    }

    return 0;
}

int vfs_unmount(const char* path)
{
    vfs_mount_t tmp_node;
    tmp_node.path = (char*)path;

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

    if (node == NULL)
    {
        return -ENOENT;
    }

    vfs_dec_node(node);

    return 0;
}

vfs_operations_t* vfs_visitor(void)
{
    return g_vfs->visitor;
}
