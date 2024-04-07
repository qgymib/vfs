#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "vfs/fs/memfs.h"
#include "utils/atomic.h"
#include "utils/defs.h"
#include "utils/strlist.h"
#include "utils/dir.h"
#include "utils/rwlock.h"
#include "utils/mutex.h"
#include "utils/map.h"

typedef struct vfs_memfs_node_dir
{
    struct vfs_memfs_node**     children;           /**< This node's children. */
    size_t                      children_sz;        /**< The number of children. */
    size_t                      children_cap;       /**< The capacity of children. */
} vfs_memfs_node_dir_t;

typedef struct vfs_memfs_node_reg
{
    uint8_t*                    data;               /**< File content. '\0' terminal is always appended. */
    uint64_t                    size;               /**< Content size in bytes, not including terminal '\0'. */
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
        vfs_memfs_node_dir_t    dir;                /**< Directory. */
        vfs_memfs_node_reg_t    reg;                /**< Regular file. */
    } data;
} vfs_memfs_node_t;

typedef struct vfs_memfs_session
{
    ev_map_node_t               node;
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

typedef struct vfs_memfs
{
    vfs_operations_t            op;                 /**< Base operations. */

    ev_map_t                    session_map;        /**< Session map. */
    vfs_mutex_t                 session_map_lock;   /**< Session map lock. */

    vfs_memfs_node_t*           root;               /**< File system tree. */
} vfs_memfs_t;

typedef struct vfs_memfs_delete_helper
{
    vfs_str_t* basename;
    vfs_stat_flag_t type;
} vfs_memfs_delete_helper_t;

static int _vfs_memfs_common_cmp_session(const ev_map_node_t* key1,
    const ev_map_node_t* key2, void* arg)
{
    (void)arg;
    vfs_memfs_session_t* session_1 = EV_CONTAINER_OF(key1, vfs_memfs_session_t, node);
    vfs_memfs_session_t* session_2 = EV_CONTAINER_OF(key2, vfs_memfs_session_t, node);
    if (session_1->data.fh == session_2->data.fh)
    {
        return 0;
    }
    return session_1->data.fh < session_2->data.fh ? -1 : 1;
}

static void _vfs_memfs_common_remove_node_from_parent(vfs_memfs_node_t* node)
{
    vfs_memfs_node_t* parent = node->parent;
    if (parent == NULL)
    {
        return;
    }
    assert(parent->stat.st_mode & VFS_S_IFDIR);

    size_t i;
    vfs_rwlock_wrlock(&parent->rwlock);
    for (i = 0; i < parent->data.dir.children_sz; i++)
    {
        if (parent->data.dir.children[i] == node)
        {
            /* Set to NULL to unlink child. */
            parent->data.dir.children[i] = NULL;

            /* Move element if necessary. */
            if (i != parent->data.dir.children_sz - 1)
            {
                size_t move_sz = parent->data.dir.children_sz - i - 1;
                move_sz = move_sz * sizeof(vfs_memfs_node_t*);

                vfs_memfs_node_t* dst_pos = parent->data.dir.children[i];
                vfs_memfs_node_t* src_pos = parent->data.dir.children[i + 1];

                memmove(dst_pos, src_pos, move_sz);
            }
            
            parent->data.dir.children_sz--;

            break;
        }
    }
    vfs_rwlock_wrunlock(&parent->rwlock);

    node->parent = NULL;
}

/**
 * @brief Release the ownership of \p node.
 * @param[in] node - The node to be released.
 * @param[in] unlink - If true, force unlink the node from its parent.
 */
static void _vfs_memfs_common_release_node(vfs_memfs_node_t* node, int unlink)
{
    if (unlink)
    {
        _vfs_memfs_common_remove_node_from_parent(node);
    }

    if (vfs_atomic_dec(&node->refcnt) != 0)
    {
        return;
    }

    _vfs_memfs_common_remove_node_from_parent(node);

    if (node->stat.st_mode & VFS_S_IFDIR)
    {
        while (node->data.dir.children_sz != 0)
        {
            vfs_memfs_node_t* child = node->data.dir.children[node->data.dir.children_sz - 1];
            _vfs_memfs_common_release_node(child, 1);

            /*
             * DO NOT REDUCE SIZE HERE!
             * The #_vfs_memfs_common_release_node() will maintain the children size.
             */
        }
        free(node->data.dir.children);
        node->data.dir.children = NULL;
        node->data.dir.children_sz = 0;
        node->data.dir.children_cap = 0;
    }
    else
    {
        free(node->data.reg.data);
        node->data.reg.data = NULL;
        node->data.reg.size = 0;
    }

    vfs_str_exit(&node->name);
    free(node);
}

/**
 * @brief Search for \p name in \p parent.
 * @param[in] parent - Parent node.
 * @param[in] name - Name to search for.
 * @return Found node, or NULL if not found. If found, the reference count will be increased.
 */
static vfs_memfs_node_t* _vfs_memfs_common_search_for(vfs_memfs_node_t* parent, const vfs_str_t* name)
{
    size_t i;
    vfs_memfs_node_t* node = NULL;

    vfs_rwlock_rdlock(&parent->rwlock);
    for (i = 0; i < parent->data.dir.children_sz; i++)
    {
        vfs_memfs_node_t* child = parent->data.dir.children[i];
        if (vfs_str_cmp2(&child->name, name) == 0)
        {
            node = child;
            (void)vfs_atomic_add(&child->refcnt);
            break;
        }
    }
    vfs_rwlock_rdunlock(&parent->rwlock);

    return node;
}

/**
 * @brief Common operation for \p path.
 * @param[in] fs - File system object.
 * @param[in] path - The path.
 * @param[in] cb - Callback function.
 * @param[in] data - Callback data.
 * @return 0 on success, or -errno on error.
 */
static int _vfs_memfs_common_op_path(vfs_memfs_t* fs, const vfs_str_t* path,
    int (*cb)(vfs_memfs_node_t* node, void* data), void* data)
{
    int ret;
    vfs_memfs_node_t* parent = fs->root;
    (void)vfs_atomic_add(&parent->refcnt);

    vfs_strlist_t path_list = vfs_str_split(path, "/", 1);
    if (path_list.num == 0)
    {
        goto do_cb;
    }

    for (size_t i = 0; i < path_list.num; i++)
    {
        vfs_str_t* name = &path_list.arr[i];
        vfs_memfs_node_t* child = _vfs_memfs_common_search_for(parent, name);
        _vfs_memfs_common_release_node(parent, 0);

        if (child == NULL)
        {
            return VFS_ENOENT;
        }
        parent = child;
    }

do_cb:
    ret = cb(parent, data);
    _vfs_memfs_common_release_node(parent, 0);
    vfs_strlist_exit(&path_list);

    return ret;
}

static int _vfs_memfs_common_node_ensure_capacity(vfs_memfs_node_t* node, size_t cap)
{
    if (node->data.dir.children_cap >= cap)
    {
        return 0;
    }

    size_t malloc_sz = cap * sizeof(vfs_memfs_node_t*);
    vfs_memfs_node_t** new_children = realloc(node->data.dir.children, malloc_sz);
    if (new_children == NULL)
    {
        return VFS_ENOMEM;
    }

    node->data.dir.children = new_children;
    node->data.dir.children_cap = cap;

    return 0;
}

static vfs_memfs_node_t* _vfs_memfs_common_new_node(vfs_memfs_node_t* parent,
    const vfs_str_t* name, vfs_stat_flag_t type)
{
    int ret;
    vfs_memfs_node_t* new_node = calloc(1, sizeof(vfs_memfs_node_t));
    if (new_node == NULL)
    {
        return NULL;
    }
    new_node->parent = parent;
    new_node->refcnt = 1;
    new_node->name = vfs_str_dup(name);
    new_node->stat.st_mode = type;
    vfs_rwlock_init(&new_node->rwlock);

    if (parent != NULL)
    {
        vfs_rwlock_wrlock(&parent->rwlock);
        do {
            ret = _vfs_memfs_common_node_ensure_capacity(parent, parent->data.dir.children_sz + 1);
            if (ret != 0)
            {
                break;
            }
            parent->data.dir.children[parent->data.dir.children_sz] = new_node;
            parent->data.dir.children_sz++;
        } while (0);
        vfs_rwlock_wrunlock(&parent->rwlock);

        if (ret != 0)
        {
            _vfs_memfs_common_release_node(new_node, 1);
            return NULL;
        }
    }

    return new_node;
}

static void _vfs_memfs_common_release_session(vfs_memfs_session_t* session)
{
    if (vfs_atomic_dec(&session->refcnt) != 0)
    {
        return;
    }

    if (session->data.node != NULL)
    {
        _vfs_memfs_common_release_node(session->data.node, 0);
        session->data.node = NULL;
    }

    vfs_mutex_exit(&session->mutex);
    free(session);
}

static int _vfs_memfs_common_op_fh(vfs_memfs_t* fs, uintptr_t fh,
    int (*cb)(vfs_memfs_session_t* session, void* data), void* data)
{
    vfs_memfs_session_t tmp_session;
    tmp_session.data.fh = fh;

    vfs_memfs_session_t* session = NULL;
    vfs_mutex_enter(&fs->session_map_lock);
    do {
        ev_map_node_t* it = vfs_map_find(&fs->session_map, &tmp_session.node);
        if (it == NULL)
        {
            break;
        }

        session = EV_CONTAINER_OF(it, vfs_memfs_session_t, node);
        (void)vfs_atomic_add(&session->refcnt);
    } while (0);
    vfs_mutex_leave(&fs->session_map_lock);

    if (session == NULL)
    {
        return VFS_EBADF;
    }

    int ret = cb(session, data);
    _vfs_memfs_common_release_session(session);

    return ret;
}

static int _vfs_memfs_common_delete_inner(vfs_memfs_node_t* node, void* data)
{
    vfs_memfs_delete_helper_t* helper = data;

    /* The reference count has been increased. */
    vfs_memfs_node_t* child = _vfs_memfs_common_search_for(node, helper->basename);
    if (child == NULL)
    {
        return VFS_ENOENT;
    }

    /* We cannot delete the wrong type. */
    if (!(child->stat.st_mode & helper->type))
    {
        _vfs_memfs_common_release_node(child, 1);
        return VFS_ENOTDIR;
    }
    /* Now we are ready to delete the node. */

    /* The first time to release reference increased by #_vfs_memfs_common_search_for(). */
    _vfs_memfs_common_release_node(child, 1);
    /* The second time to release reference to actually delete it. */
    _vfs_memfs_common_release_node(child, 1);

    return 0;
}

static int _vfs_memfs_common_delete(vfs_memfs_t* fs, const vfs_str_t* path,
    vfs_stat_flag_t type)
{
    int ret;
    vfs_str_t basename = VFS_STR_INIT;
    vfs_str_t parent = vfs_path_parent(path, &basename);
    {
        vfs_memfs_delete_helper_t helper = { &basename, type };
        ret = _vfs_memfs_common_op_path(fs, &parent, _vfs_memfs_common_delete_inner, &helper);
    }
    vfs_str_exit(&basename);
    vfs_str_exit(&parent);

    return ret;
}

//////////////////////////////////////////////////////////////////////////
// destroy
//////////////////////////////////////////////////////////////////////////

static void _vfs_memfs_cleanup_session(vfs_memfs_t* fs)
{
    ev_map_node_t* it;

    vfs_mutex_enter(&fs->session_map_lock);
    while ((it = vfs_map_begin(&fs->session_map))!= NULL)
    {
        vfs_memfs_session_t* session = EV_CONTAINER_OF(it, vfs_memfs_session_t, node);
        vfs_map_erase(&fs->session_map, it);
        vfs_mutex_leave(&fs->session_map_lock);
        {
            _vfs_memfs_common_release_session(session);
        }
        vfs_mutex_enter(&fs->session_map_lock);
    }
    vfs_mutex_leave(&fs->session_map_lock);
}

static void _vfs_memfs_destroy(struct vfs_operations* thiz)
{
    vfs_memfs_t* fs = EV_CONTAINER_OF(thiz, vfs_memfs_t, op);

    _vfs_memfs_cleanup_session(fs);

    if (fs->root != NULL)
    {
        _vfs_memfs_common_release_node(fs->root, 1);
        fs->root = NULL;
    }

    vfs_mutex_exit(&fs->session_map_lock);
    free(fs);
}

//////////////////////////////////////////////////////////////////////////
// ls
//////////////////////////////////////////////////////////////////////////

typedef struct vfs_memfs_ls_helper
{
    vfs_ls_cb   fn;
    void*       data;
} vfs_memfs_ls_helper_t;

static int _vfs_memfs_ls_inner(vfs_memfs_node_t* node, void* data)
{
    size_t i;
    int ret;
    vfs_memfs_ls_helper_t* helper = data;

    vfs_rwlock_wrlock(&node->rwlock);
    for (i = 0; i < node->data.dir.children_sz; i++)
    {
        vfs_memfs_node_t* child = node->data.dir.children[i];

        vfs_rwlock_wrunlock(&node->rwlock);
        {
            ret = helper->fn(child->name.str, &child->stat, helper->data);
        }
        vfs_rwlock_wrlock(&node->rwlock);

        if (ret != 0)
        {
            break;
        }
    }
    vfs_rwlock_wrunlock(&node->rwlock);

    return 0;
}

static int _vfs_memfs_ls(struct vfs_operations* thiz, const char* path,
    vfs_ls_cb fn, void* data)
{
    vfs_memfs_t* fs = EV_CONTAINER_OF(thiz, vfs_memfs_t, op);
    vfs_str_t path_str = vfs_str_from_static1(path);

    vfs_memfs_ls_helper_t helper = { fn, data };
    return _vfs_memfs_common_op_path(fs, &path_str, _vfs_memfs_ls_inner, &helper);
}

//////////////////////////////////////////////////////////////////////////
// stat
//////////////////////////////////////////////////////////////////////////

typedef struct vfs_memfs_stat_helper
{
    vfs_stat_t*     info;
} vfs_memfs_stat_helper_t;

static int _vfs_memfs_stat_inner(vfs_memfs_node_t* node, void* data)
{
    vfs_memfs_stat_helper_t* helper = data;

    vfs_rwlock_rdlock(&node->rwlock);
    {
        *helper->info = node->stat;
    }
    vfs_rwlock_rdunlock(&node->rwlock);
    return 0;
}

static int _vfs_memfs_stat(struct vfs_operations* thiz, const char* path,
    vfs_stat_t* info)
{
    vfs_memfs_t* fs = EV_CONTAINER_OF(thiz, vfs_memfs_t, op);
    vfs_str_t path_str = vfs_str_from_static1(path);

    vfs_memfs_stat_helper_t helper = { info };
    return _vfs_memfs_common_op_path(fs, &path_str, _vfs_memfs_stat_inner, &helper);
}

//////////////////////////////////////////////////////////////////////////
// open
//////////////////////////////////////////////////////////////////////////

typedef struct vfs_mmefs_open_searcher
{
    vfs_memfs_node_t* node;
} vfs_mmefs_open_searcher_t;

static int _vfs_memfs_open_searcher(vfs_memfs_node_t* node, void* data)
{
    vfs_mmefs_open_searcher_t* searcher = data;

    searcher->node = node;
    (void)vfs_atomic_add(&node->refcnt);

    return 0;
}

static int _vfs_memfs_open_exist(vfs_memfs_t* fs, vfs_memfs_node_t* node, uintptr_t* fh, uint64_t flags)
{
    vfs_memfs_session_t* session = calloc(1, sizeof(vfs_memfs_session_t));
    if (session == NULL)
    {
        return VFS_ENOMEM;
    }
    session->refcnt = 1;
    vfs_mutex_init(&session->mutex);
    session->data.fh = (uintptr_t)session;
    session->data.flags = flags;
    session->data.fpos = 0;
    session->data.node = node;
    (void)vfs_atomic_add(&node->refcnt);

    /* Handle APPEND flag. */
    if (flags & VFS_O_APPEND)
    {
        session->data.fpos = UINT64_MAX;
    }
    /* Handle TRUNCATE flag. */
    if (flags & VFS_O_TRUNCATE)
    {
        void* buf;
        vfs_rwlock_wrlock(&node->rwlock);
        {
            buf = node->data.reg.data;
            node->data.reg.data = NULL;
            node->data.reg.size = 0;
        }
        vfs_rwlock_wrunlock(&node->rwlock);
        free(buf);
    }

    /* Save session. */
    {
        ev_map_node_t* orig;
        vfs_mutex_enter(&fs->session_map_lock);
        {
            orig = vfs_map_insert(&fs->session_map, &session->node);
        }
        vfs_mutex_leave(&fs->session_map_lock);
        if (orig != NULL)
        {
            abort();
        }
    }

    *fh = session->data.fh;
    return 0;
}

static int _vfs_memfs_open_inner(vfs_memfs_t* fs, vfs_memfs_node_t* parent,
    uintptr_t* fh, const vfs_str_t* name, uint64_t flags)
{
    int ret = 0;

    /* Get node. */
    vfs_memfs_node_t* child = _vfs_memfs_common_search_for(parent, name);
    if (child == NULL)
    {
        if (!(flags & VFS_O_CREATE))
        {
            return VFS_ENOENT;
        }

        if ((child = _vfs_memfs_common_new_node(parent, name, VFS_S_IFREG)) == NULL)
        {
            return VFS_ENOMEM;
        }
        (void)vfs_atomic_add(&child->refcnt);
    }

    /* Check type. */
    if (!(child->stat.st_mode & VFS_S_IFREG))
    {
        ret = VFS_EISDIR;
        goto finish;
    }

    /* Open file. */
    ret = _vfs_memfs_open_exist(fs, child, fh, flags);

finish:
    _vfs_memfs_common_release_node(child, 0);
    return ret;
}

static int _vfs_memfs_open(struct vfs_operations* thiz, uintptr_t* fh, const char* path, uint64_t flags)
{
    int ret = 0;
    vfs_memfs_t* fs = EV_CONTAINER_OF(thiz, vfs_memfs_t, op);
    vfs_str_t path_str = vfs_str_from_static1(path);
    vfs_str_t basename = VFS_STR_INIT;
    vfs_str_t parent_path = vfs_path_parent(&path_str, &basename);

    /* Search for parent node. */
    vfs_memfs_node_t* parent = NULL;
    if (vfs_str_cmp1(&parent_path, "/") != 0)
    {
        vfs_mmefs_open_searcher_t searcher = { NULL };
        ret = _vfs_memfs_common_op_path(fs, &parent_path, _vfs_memfs_open_searcher, &searcher);
        if (ret != 0)
        {
            goto finish;
        }
        parent = searcher.node;
    }
    else
    {
        parent = fs->root;
        (void)vfs_atomic_add(&parent->refcnt);
    }

    ret = _vfs_memfs_open_inner(fs, parent, fh, &basename, flags);
    _vfs_memfs_common_release_node(parent, 0);

finish:
    vfs_str_exit(&parent_path);
    vfs_str_exit(&basename);
    return ret;
}

//////////////////////////////////////////////////////////////////////////
// close
//////////////////////////////////////////////////////////////////////////

static int _vfs_memfs_close(struct vfs_operations* thiz, uintptr_t fh)
{
    vfs_memfs_t* fs = EV_CONTAINER_OF(thiz, vfs_memfs_t, op);

    vfs_memfs_session_t tmp_session;
    tmp_session.data.fh = fh;

    vfs_memfs_session_t* session = NULL;
    vfs_mutex_enter(&fs->session_map_lock);
    do {
        ev_map_node_t* it = vfs_map_find(&fs->session_map, &tmp_session.node);
        if (it == NULL)
        {
            break;
        }

        session = EV_CONTAINER_OF(it, vfs_memfs_session_t, node);
        vfs_map_erase(&fs->session_map, it);
    } while (0);
    vfs_mutex_leave(&fs->session_map_lock);

    if (session == NULL)
    {
        return VFS_EBADF;
    }

    _vfs_memfs_common_release_session(session);
    return 0;
}

//////////////////////////////////////////////////////////////////////////
// seek
//////////////////////////////////////////////////////////////////////////

typedef struct vfs_memfs_seek_helper
{
    int64_t offset;
    int     whence;
} vfs_memfs_seek_helper_t;

static int _vfs_memfs_seek_inner(vfs_memfs_session_t* session, void* data)
{
    vfs_memfs_seek_helper_t* helper = data;
    vfs_memfs_node_t* node = session->data.node;

    if (helper->whence == VFS_SEEK_SET)
    {
        vfs_mutex_enter(&session->mutex);
        {
            session->data.fpos = helper->offset;
        }
        vfs_mutex_leave(&session->mutex);
        return 0;
    }

    if (helper->whence == VFS_SEEK_CUR)
    {
        if (session->data.fpos == UINT64_MAX)
        {
            goto handle_seek_end;
        }
        vfs_mutex_enter(&session->mutex);
        {
            session->data.fpos += helper->offset;
        }
        vfs_mutex_leave(&session->mutex);
        return 0;
    }
    /* Now whence is #VFS_SEEK_END. */

    /* offset == 0 is special, it means always append to end of file. */
    if (helper->offset == 0)
    {
        vfs_mutex_enter(&session->mutex);
        {
            session->data.fpos = UINT64_MAX;
        }
        vfs_mutex_leave(&session->mutex);
        return 0;
    }

    /* In other condition, we need to have absolute position. */
handle_seek_end:
    vfs_mutex_enter(&session->mutex);
    vfs_rwlock_rdlock(&node->rwlock);
    {
        session->data.fpos = node->data.reg.size + helper->offset;
    }
    vfs_rwlock_rdunlock(&node->rwlock);
    vfs_mutex_leave(&session->mutex);
    
    return 0;
}

static int64_t _vfs_memfs_seek(struct vfs_operations* thiz, uintptr_t fh, int64_t offset, int whence)
{
    vfs_memfs_t* fs = EV_CONTAINER_OF(thiz, vfs_memfs_t, op);

    vfs_memfs_seek_helper_t helper = { offset, whence };
    return _vfs_memfs_common_op_fh(fs, fh, _vfs_memfs_seek_inner, &helper);
}

//////////////////////////////////////////////////////////////////////////
// read
//////////////////////////////////////////////////////////////////////////

typedef struct vfs_memfs_read_helper
{
    void*   buf;
    size_t  len;
} vfs_memfs_read_helper_t;

static int _vfs_memfs_read_inner(vfs_memfs_session_t* session, void* data)
{
    int ret = 0;
    vfs_memfs_read_helper_t* helper = data;
    vfs_memfs_node_t* node = session->data.node;

    vfs_mutex_enter(&session->mutex);
    vfs_rwlock_rdlock(&node->rwlock);
    do 
    {
        if (session->data.fpos >= node->data.reg.size)
        {
            ret = VFS_EOF;
            break;
        }

        size_t left_sz = node->data.reg.size - session->data.fpos;
        ret = (int)min(left_sz, helper->len);
        memcpy(helper->buf, node->data.reg.data, ret);
        session->data.fpos += ret;
    } while (0);
    vfs_rwlock_rdunlock(&node->rwlock);
    vfs_mutex_leave(&session->mutex);
    
    return ret;
}

static int _vfs_memfs_read(struct vfs_operations* thiz, uintptr_t fh, void* buf, size_t len)
{
    vfs_memfs_t* fs = EV_CONTAINER_OF(thiz, vfs_memfs_t, op);
    vfs_memfs_read_helper_t helper = { buf, len };
    return _vfs_memfs_common_op_fh(fs, fh, _vfs_memfs_read_inner, &helper);
}

//////////////////////////////////////////////////////////////////////////
// write
//////////////////////////////////////////////////////////////////////////

typedef struct vfs_memfs_write_helper
{
    const void* buf;
    size_t      len;
} vfs_memfs_write_helper_t;

static int _vfs_memfs_write_append(vfs_memfs_node_t* node, const void* buf, size_t len)
{
    size_t old_sz = node->data.reg.size;
    size_t new_sz = old_sz + len;
    uint8_t* new_buf = realloc(node->data.reg.data, new_sz + 1);
    if (new_buf == NULL)
    {
        return VFS_ENOMEM;
    }
    node->data.reg.data = new_buf;
    node->data.reg.size = new_sz;
    memcpy(node->data.reg.data + old_sz, buf, len);
    node->data.reg.data[new_sz] = '\0';

    return (int)len;
}

static int _vfs_memfs_write_pos(vfs_memfs_session_t* session, vfs_memfs_node_t* node, const void* buf, size_t len)
{
    if (session->data.fpos + len < node->data.reg.size)
    {
        memcpy(node->data.reg.data + session->data.fpos, buf, len);
        session->data.fpos += len;
        return (int)len;
    }

    size_t new_sz = session->data.fpos + len;
    uint8_t* new_buf = realloc(node->data.reg.data, new_sz + 1);
    if (new_buf == NULL)
    {
        return VFS_ENOMEM;
    }
    node->data.reg.data = new_buf;

    if (session->data.fpos >= node->data.reg.size)
    {
        memset(node->data.reg.data + node->data.reg.size, 0, session->data.fpos - node->data.reg.size);
    }
    memcpy(new_buf + session->data.fpos, buf, len);
    node->data.reg.size = new_sz;
    node->data.reg.data[node->data.reg.size] = '\0';
    
    session->data.fpos += len;
    return (int)len;
}

static int _vfs_memfs_write_inner(vfs_memfs_session_t* session, void* data)
{
    int ret = 0;
    vfs_memfs_write_helper_t* helper = data;
    vfs_memfs_node_t* node = session->data.node;

    vfs_mutex_enter(&session->mutex);
    vfs_rwlock_wrlock(&node->rwlock);
    do
    {
        if (session->data.fpos == UINT64_MAX)
        {
            ret = _vfs_memfs_write_append(node, helper->buf, helper->len);
            break;
        }
        ret = _vfs_memfs_write_pos(session, node, helper->buf, helper->len);
    } while (0);
    vfs_rwlock_wrunlock(&node->rwlock);
    vfs_mutex_leave(&session->mutex);

    return ret;
}

static int _vfs_memfs_write(struct vfs_operations* thiz, uintptr_t fh, const void* buf, size_t len)
{
    vfs_memfs_t* fs = EV_CONTAINER_OF(thiz, vfs_memfs_t, op);
    vfs_memfs_write_helper_t helper = { buf, len };
    return _vfs_memfs_common_op_fh(fs, fh, _vfs_memfs_write_inner, &helper);
}

//////////////////////////////////////////////////////////////////////////
// mkdir
//////////////////////////////////////////////////////////////////////////

static int _vfs_memfs_mkdir_inner(vfs_memfs_node_t* node, void* data)
{
    vfs_str_t* basename = data;

    vfs_memfs_node_t* child = _vfs_memfs_common_search_for(node, basename);
    if (child != NULL)
    {
        _vfs_memfs_common_release_node(child, 0);
        return VFS_EALREADY;
    }

    if ((child = _vfs_memfs_common_new_node(node, basename, VFS_S_IFDIR)) == NULL)
    {
        return VFS_ENOMEM;
    }

    return 0;
}

static int _vfs_memfs_mkdir(struct vfs_operations* thiz, const char* path)
{
    int ret = 0;
    vfs_memfs_t* fs = EV_CONTAINER_OF(thiz, vfs_memfs_t, op);
    vfs_str_t path_str = vfs_str_from_static1(path);

    vfs_str_t basename = VFS_STR_INIT;
    vfs_str_t parent = vfs_path_parent(&path_str, &basename);
    {
        ret = _vfs_memfs_common_op_path(fs, &parent, _vfs_memfs_mkdir_inner, &basename);
    }
    vfs_str_exit(&basename);
    vfs_str_exit(&parent);

    return ret;
}

//////////////////////////////////////////////////////////////////////////
// rmdir
//////////////////////////////////////////////////////////////////////////

static int _vfs_memfs_rmdir(struct vfs_operations* thiz, const char* path)
{
    vfs_memfs_t* fs = EV_CONTAINER_OF(thiz, vfs_memfs_t, op);
    vfs_str_t path_str = vfs_str_from_static1(path);
    return _vfs_memfs_common_delete(fs, &path_str, VFS_S_IFDIR);
}

//////////////////////////////////////////////////////////////////////////
// unlink
//////////////////////////////////////////////////////////////////////////

static int _vfs_memfs_unlink(struct vfs_operations* thiz, const char* path)
{
    vfs_memfs_t* fs = EV_CONTAINER_OF(thiz, vfs_memfs_t, op);
    vfs_str_t path_str = vfs_str_from_static1(path);
    return _vfs_memfs_common_delete(fs, &path_str, VFS_S_IFREG);
}

//////////////////////////////////////////////////////////////////////////
// API
//////////////////////////////////////////////////////////////////////////

int vfs_make_memory(vfs_operations_t** fs)
{
    vfs_memfs_t* memfs = calloc(1, sizeof(vfs_memfs_t));
    if (memfs == NULL)
    {
        return VFS_ENOMEM;
    }

    memfs->op.destroy = _vfs_memfs_destroy;
    memfs->op.ls = _vfs_memfs_ls;
    memfs->op.stat = _vfs_memfs_stat;
    memfs->op.open = _vfs_memfs_open;
    memfs->op.close = _vfs_memfs_close;
    memfs->op.seek = _vfs_memfs_seek;
    memfs->op.read = _vfs_memfs_read;
    memfs->op.write = _vfs_memfs_write;
    memfs->op.mkdir = _vfs_memfs_mkdir;
    memfs->op.rmdir = _vfs_memfs_rmdir;
    memfs->op.unlink = _vfs_memfs_unlink;

    vfs_map_init(&memfs->session_map, _vfs_memfs_common_cmp_session, NULL);
    vfs_mutex_init(&memfs->session_map_lock);

    vfs_str_t name = vfs_str_from_static1("");
    if ((memfs->root = _vfs_memfs_common_new_node(NULL, &name, VFS_S_IFDIR)) == NULL)
    {
        _vfs_memfs_destroy(&memfs->op);
        return VFS_ENOMEM;
    }

    *fs = &memfs->op;
    return 0;
}
