#include <stdlib.h>
#include <errno.h>
#include "utils/defs.h"
#include "vfs_visitor.h"

static void _vfs_visitor_dec_session(vfs_session_t* session)
{
    if (vfs_atomic_dec(&session->refcnt) != 0)
    {
        return;
    }

    if (session->mount != NULL)
    {
        vfs_release_mount(session->mount);
        session->mount = NULL;
    }
    free(session);
}

static void _vfs_visitor_cleanup_session(vfs_visitor_t* visitor)
{
    ev_map_node_t* it;

    vfs_rwlock_wrlock(&visitor->session_map_lock);
    while ((it = vfs_map_begin(&visitor->session_map)) != NULL)
    {
        vfs_session_t* session = EV_CONTAINER_OF(it, vfs_session_t, node);
        vfs_map_erase(&visitor->session_map, it);

        vfs_rwlock_wrunlock(&visitor->session_map_lock);
        {
            _vfs_visitor_dec_session(session);
        }
        vfs_rwlock_wrlock(&visitor->session_map_lock);

    }
    vfs_rwlock_wrunlock(&visitor->session_map_lock);
}

static int _vfs_visitor_cmp_session(const ev_map_node_t* key1, const ev_map_node_t* key2, void* arg)
{
    (void)arg;
    vfs_session_t* session1 = EV_CONTAINER_OF(key1, vfs_session_t, node);
    vfs_session_t* session2 = EV_CONTAINER_OF(key2, vfs_session_t, node);
    if (session1->fake == session2->fake)
    {
        return 0;
    }
    return session1->fake < session2->fake ? -1 : 1;
}

static int _vfs_visitor_fh(vfs_visitor_t* visitor, uintptr_t fh,
    int (*cb)(vfs_session_t* session, void* data), void* data)
{
    vfs_session_t tmp_session;
    tmp_session.fake = fh;

    vfs_session_t* session = NULL;
    vfs_rwlock_rdlock(&visitor->session_map_lock);
    do {
        ev_map_node_t* it = vfs_map_find(&visitor->session_map, &tmp_session.node);
        if (it == NULL)
        {
            break;
        }
        session = EV_CONTAINER_OF(it, vfs_session_t, node);
        (void)vfs_atomic_add(&session->refcnt);
    } while (0);
    vfs_rwlock_rdunlock(&visitor->session_map_lock);

    if (session == NULL)
    {
        return -ENOENT;
    }
    vfs_operations_t* op = session->mount->op;

    int ret;
    if (op == NULL)
    {
        ret = VFS_ENOSYS;
        goto finish;
    }

    ret = cb(session, data);

finish:
    _vfs_visitor_dec_session(session);
    return ret;
}

//////////////////////////////////////////////////////////////////////////
// destroy
//////////////////////////////////////////////////////////////////////////

static void _vfs_visitor_destroy(struct vfs_operations* thiz)
{
    vfs_visitor_t* visitor = EV_CONTAINER_OF(thiz, vfs_visitor_t, op);

    _vfs_visitor_cleanup_session(visitor);
    vfs_rwlock_exit(&visitor->session_map_lock);

    free(visitor);
}

//////////////////////////////////////////////////////////////////////////
// ls
//////////////////////////////////////////////////////////////////////////

typedef struct vfs_ls_helper
{
    vfs_ls_cb           fn;
    void*               data;
} vfs_ls_helper_t;

static int _vfs_visitor_ls_inner(vfs_mount_t* fs, const vfs_str_t* path, void* data)
{
    vfs_ls_helper_t* helper = data;
    if (fs->op->ls == NULL)
    {
        return VFS_ENOSYS;
    }

    return fs->op->ls(fs->op, path->str, helper->fn, helper->data);
}

static int _vfs_visitor_ls(struct vfs_operations* thiz, const char* path, vfs_ls_cb fn, void* data)
{
    (void)thiz;
    vfs_ls_helper_t helper = { fn, data };

    vfs_str_t path_str = vfs_str_from_static1(path);
    return vfs_access_mount(&path_str, _vfs_visitor_ls_inner, &helper);
}

//////////////////////////////////////////////////////////////////////////
// stat
//////////////////////////////////////////////////////////////////////////

typedef struct vfs_visitor_stat_helper
{
    vfs_stat_t* info;
} vfs_visitor_stat_helper_t;

static int _vfs_visitor_stat_inner(vfs_mount_t* fs, const vfs_str_t* path, void* data)
{
    vfs_visitor_stat_helper_t* helper = data;
    vfs_operations_t* op = fs->op;

    if (op->stat == NULL)
    {
        return VFS_ENOSYS;
    }

    return op->stat(op, path->str, helper->info);
}

static int _vfs_visitor_stat(struct vfs_operations* thiz, const char* path, vfs_stat_t* info)
{
    (void)thiz;
    vfs_visitor_stat_helper_t helper = { info };

    vfs_str_t path_str = vfs_str_from_static1(path);
    return vfs_access_mount(&path_str, _vfs_visitor_stat_inner, &helper);
}

//////////////////////////////////////////////////////////////////////////
// open
//////////////////////////////////////////////////////////////////////////

typedef struct vfs_open_helper
{
    vfs_visitor_t*      belong;
    uintptr_t*          fh;
    uint64_t            flags;
} vfs_open_helper_t;

static int _vfs_visitor_open_inner(vfs_mount_t* fs, const vfs_str_t* path, void* data)
{
    int ret;
    vfs_open_helper_t* helper = data;
    vfs_operations_t* op = fs->op;
    vfs_visitor_t* visitor = helper->belong;

    if (op->open == NULL)
    {
        return VFS_ENOSYS;
    }

    vfs_session_t* session = malloc(sizeof(vfs_session_t));
    if (session == NULL)
    {
        return -ENOMEM;
    }
    session->refcnt = 1;
    session->fake = vfs_atomic64_add(&visitor->fh_gen);
    session->real = 0;
    session->mount = fs;
    (void)vfs_atomic_add(&fs->refcnt);

    if ((ret = op->open(op, &session->real, path->str, helper->flags)) != 0)
    {
        _vfs_visitor_dec_session(session);
        return ret;
    }

    ev_map_node_t* orig;
    vfs_rwlock_wrlock(&visitor->session_map_lock);
    {
        orig = vfs_map_insert(&visitor->session_map, &session->node);
    }
    vfs_rwlock_wrunlock(&visitor->session_map_lock);

    if (orig != NULL)
    {
        _vfs_visitor_dec_session(session);
        return -EOVERFLOW;
    }

    *helper->fh = session->fake;

    return 0;
}

static int _vfs_visitor_open(struct vfs_operations* thiz, uintptr_t* fh, const char* path, uint64_t flags)
{
    vfs_visitor_t* visitor = EV_CONTAINER_OF(thiz, vfs_visitor_t, op);
    vfs_open_helper_t helper = { visitor, fh, flags };

    /* #VFS_O_APPEND and #VFS_O_TRUNCATE cannot be both exist. */
    if ((flags & VFS_O_APPEND) && (flags & VFS_O_TRUNCATE))
    {
        return -EINVAL;
    }

    vfs_str_t path_str = vfs_str_from_static1(path);
    return vfs_access_mount(&path_str, _vfs_visitor_open_inner, &helper);
}

//////////////////////////////////////////////////////////////////////////
// close
//////////////////////////////////////////////////////////////////////////

static int _vfs_visitor_close(struct vfs_operations* thiz, uintptr_t fh)
{
    vfs_visitor_t* visitor = EV_CONTAINER_OF(thiz, vfs_visitor_t, op);

    vfs_session_t tmp_session;
    tmp_session.fake = fh;

    vfs_session_t* session = NULL;
    vfs_rwlock_wrlock(&visitor->session_map_lock);
    do {
        ev_map_node_t* it = vfs_map_find(&visitor->session_map, &tmp_session.node);
        if (it == NULL)
        {
            break;
        }

        session = EV_CONTAINER_OF(it, vfs_session_t, node);
        vfs_map_erase(&visitor->session_map, it);
    } while (0);
    vfs_rwlock_wrunlock(&visitor->session_map_lock);

    if (session == NULL)
    {
        return -ENOENT;
    }

    _vfs_visitor_dec_session(session);
    return 0;
}

//////////////////////////////////////////////////////////////////////////
// seek
//////////////////////////////////////////////////////////////////////////

typedef struct vfs_visitor_seek_helper
{
    int64_t offset;
    int     whence;
    int64_t ret;
} vfs_visitor_seek_helper_t;

static int _vfs_visitor_seek_inner(vfs_session_t* session, void* data)
{
    vfs_visitor_seek_helper_t* helper = data;
    vfs_operations_t* op = session->mount->op;

    if (op->seek == NULL)
    {
        return VFS_ENOSYS;
    }

    helper->ret = op->seek(op, session->real, helper->offset, helper->whence);
    return 0;
}

static int64_t _vfs_visitor_seek(struct vfs_operations* thiz, uintptr_t fh, int64_t offset, int whence)
{
    vfs_visitor_t* visitor = EV_CONTAINER_OF(thiz, vfs_visitor_t, op);
    vfs_visitor_seek_helper_t helper = { offset, whence, 0 };

    int ret = _vfs_visitor_fh(visitor, fh, _vfs_visitor_seek_inner, &helper);
    if (ret < 0)
    {
        return ret;
    }
    return helper.ret;
}

//////////////////////////////////////////////////////////////////////////
// read
//////////////////////////////////////////////////////////////////////////

typedef struct vfs_read_helper
{
    void*               buf;
    size_t              len;
} vfs_read_helper_t;

static int _vfs_visitor_read_inner(vfs_session_t* session, void* data)
{
    vfs_operations_t* op = session->mount->op;
    vfs_read_helper_t* helper = data;

    if (op->read == NULL)
    {
        return VFS_ENOSYS;
    }

    return op->read(op, session->real, helper->buf, helper->len);
}

static int _vfs_visitor_read(struct vfs_operations* thiz, uintptr_t fh, void* buf, size_t len)
{
    vfs_visitor_t* visitor = EV_CONTAINER_OF(thiz, vfs_visitor_t, op);
    vfs_read_helper_t helper = { buf, len };
    return _vfs_visitor_fh(visitor, fh, _vfs_visitor_read_inner, &helper);
}

//////////////////////////////////////////////////////////////////////////
// write
//////////////////////////////////////////////////////////////////////////

typedef struct vfs_write_helper
{
    const void*         buf;
    size_t              len;
} vfs_write_helper_t;

static int _vfs_visitor_write_inner(vfs_session_t* session, void* data)
{
    vfs_operations_t* op = session->mount->op;
    vfs_write_helper_t* helper = data;

    if (op->write == NULL)
    {
        return VFS_ENOSYS;
    }

    return op->write(op, session->real, helper->buf, helper->len);
}

static int _vfs_visitor_write(struct vfs_operations* thiz, uintptr_t fh, const void* buf, size_t len)
{
    vfs_visitor_t* visitor = EV_CONTAINER_OF(thiz, vfs_visitor_t, op);
    vfs_write_helper_t helper = { buf, len };
    return _vfs_visitor_fh(visitor, fh, _vfs_visitor_write_inner, &helper);
}

//////////////////////////////////////////////////////////////////////////
// rm
//////////////////////////////////////////////////////////////////////////

typedef struct vfs_visitor_rm_helper
{
    int flags;
} vfs_visitor_rm_helper_t;

static int _vfs_visitor_rm_inner(vfs_mount_t* fs, const vfs_str_t* path, void* data)
{
    vfs_visitor_rm_helper_t* helper = data;
    vfs_operations_t* op = fs->op;
    if (op->rm == NULL)
    {
        return VFS_ENOSYS;
    }

    return op->rm(op, path->str, helper->flags);
}

static int _vfs_visitor_rm(struct vfs_operations* thiz, const char* path, int flags)
{
    (void)thiz;
    vfs_visitor_rm_helper_t helper = { flags };

    vfs_str_t path_str = vfs_str_from_static1(path);
    return vfs_access_mount(&path_str, _vfs_visitor_rm_inner, &helper);
}

//////////////////////////////////////////////////////////////////////////
// mkdir
//////////////////////////////////////////////////////////////////////////

static int _vfs_visitor_mkdir_inner(vfs_mount_t* fs, const vfs_str_t* path, void* data)
{
    (void)data;

    if (fs->op->mkdir == NULL)
    {
        return VFS_ENOSYS;
    }

    return fs->op->mkdir(fs->op, path->str);
}

static int _vfs_visitor_mkdir(struct vfs_operations* thiz, const char* path)
{
    (void)thiz;
    vfs_str_t path_str = vfs_str_from_static1(path);
    return vfs_access_mount(&path_str, _vfs_visitor_mkdir_inner, NULL);
}

vfs_operations_t* vfs_create_visitor(void)
{
    vfs_visitor_t* visitor = malloc(sizeof(vfs_visitor_t));
    if (visitor == NULL)
    {
        return NULL;
    }

    visitor->op.destroy = _vfs_visitor_destroy;
    visitor->op.ls = _vfs_visitor_ls;
    visitor->op.stat = _vfs_visitor_stat;
    visitor->op.open = _vfs_visitor_open;
    visitor->op.close = _vfs_visitor_close;
    visitor->op.seek = _vfs_visitor_seek;
    visitor->op.read = _vfs_visitor_read;
    visitor->op.write = _vfs_visitor_write;
    visitor->op.rm = _vfs_visitor_rm;
    visitor->op.mkdir = _vfs_visitor_mkdir;

    visitor->fh_gen = 1;
    vfs_map_init(&visitor->session_map, _vfs_visitor_cmp_session, NULL);
    vfs_rwlock_init(&visitor->session_map_lock);

    return &visitor->op;
}
