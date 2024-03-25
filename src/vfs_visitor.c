#include <stdlib.h>
#include <errno.h>
#include "utils/defs.h"
#include "vfs_visitor.h"

typedef struct vfs_ls_helper
{
    vfs_ls_cb           fn;
    void*               data;
} vfs_ls_helper_t;

typedef struct vfs_open_helper
{
    vfs_visitor_t*      belong;
    uintptr_t*          fh;
    uint64_t            flags;
} vfs_open_helper_t;

typedef struct vfs_read_helper
{
    void*               buf;
    size_t              len;
} vfs_read_helper_t;

typedef struct vfs_write_helper
{
    const void*         buf;
    size_t              len;
} vfs_write_helper_t;

static void _vfs_dec_session(vfs_session_t* session)
{
    if (vfs_atomic_dec(&session->refcnt) != 0)
    {
        return;
    }

    if (session->mount != NULL)
    {
        vfs_dec_node(session->mount);
        session->mount = NULL;
    }
    free(session);
}

static void _vfs_cleanup_session(vfs_visitor_t* visitor)
{
    ev_map_node_t* it;

    vfs_rwlock_wrlock(&visitor->session_map_lock);
    while ((it = vfs_map_begin(&visitor->session_map)) != NULL)
    {
        vfs_session_t* session = EV_CONTAINER_OF(it, vfs_session_t, node);
        vfs_map_erase(&visitor->session_map, it);

        vfs_rwlock_wrunlock(&visitor->session_map_lock);
        {
            _vfs_dec_session(session);
        }
        vfs_rwlock_wrlock(&visitor->session_map_lock);

    }
    vfs_rwlock_wrunlock(&visitor->session_map_lock);
}

static void _vfs_visitor_destroy(struct vfs_operations* thiz)
{
    vfs_visitor_t* visitor = EV_CONTAINER_OF(thiz, vfs_visitor_t, op);

    _vfs_cleanup_session(visitor);
    vfs_rwlock_exit(&visitor->session_map_lock);

    free(visitor);
}

static int _vfs_on_ls(vfs_mount_t* fs, const char* path, void* data)
{
    vfs_ls_helper_t* helper = data;
    if (fs->op->ls == NULL)
    {
        return -ENOSYS;
    }

    return fs->op->ls(fs->op, path, helper->fn, helper->data);
}

static int _vfs_visitor_ls(struct vfs_operations* thiz, const char* path, vfs_ls_cb fn, void* data)
{
    (void)thiz;
    vfs_ls_helper_t helper = { fn, data };

    return vfs_op_safe(path, _vfs_on_ls, &helper);
}

static int _vfs_on_open(vfs_mount_t* fs, const char* path, void* data)
{
    int ret;
    vfs_open_helper_t* helper = data;
    vfs_operations_t* op = fs->op;
    vfs_visitor_t* visitor = helper->belong;

    if (op->open == NULL)
    {
        return -ENOSYS;
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

    if ((ret = op->open(op, &session->real, path, helper->flags)) != 0)
    {
        _vfs_dec_session(session);
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
        _vfs_dec_session(session);
        return -EOVERFLOW;
    }

    return 0;
}

static int _vfs_visitor_open(struct vfs_operations* thiz, uintptr_t* fh, const char* path, uint64_t flags)
{
    vfs_visitor_t* visitor = EV_CONTAINER_OF(thiz, vfs_visitor_t, op);
    vfs_open_helper_t helper = { visitor, fh, flags };
    return vfs_op_safe(path, _vfs_on_open, &helper);
}

static int _vfs_map_cmp_session(const ev_map_node_t* key1, const ev_map_node_t* key2, void* arg)
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

    _vfs_dec_session(session);
    return 0;
}

static int _vfs_on_read(vfs_session_t* session, void* data)
{
    vfs_operations_t* op = session->mount->op;
    vfs_read_helper_t* helper = data;

    if (op->read == NULL)
    {
        return -ENOSYS;
    }

    return op->read(op, session->real, helper->buf, helper->len);
}

static int _vfs_op_fh_common(vfs_visitor_t* visitor, uintptr_t fh, int (*cb)(vfs_session_t* session, void* data), void* data)
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
        ret = -ENOSYS;
        goto finish;
    }

    ret = cb(session, data);

finish:
    _vfs_dec_session(session);
    return ret;
}

static int _vfs_visitor_read(struct vfs_operations* thiz, uintptr_t fh, void* buf, size_t len)
{
    vfs_visitor_t* visitor = EV_CONTAINER_OF(thiz, vfs_visitor_t, op);
    vfs_read_helper_t helper = { buf, len };
    return _vfs_op_fh_common(visitor, fh, _vfs_on_read, &helper);
}

static int _vfs_on_write(vfs_session_t* session, void* data)
{
    vfs_operations_t* op = session->mount->op;
    vfs_write_helper_t* helper = data;

    if (op->write == NULL)
    {
        return -ENOSYS;
    }

    return op->write(op, session->real, helper->buf, helper->len);
}

static int _vfs_visitor_write(struct vfs_operations* thiz, uintptr_t fh, const void* buf, size_t len)
{
    vfs_visitor_t* visitor = EV_CONTAINER_OF(thiz, vfs_visitor_t, op);
    vfs_write_helper_t helper = { buf, len };
    return _vfs_op_fh_common(visitor, fh, _vfs_on_write, &helper);
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
    visitor->op.open = _vfs_visitor_open;
    visitor->op.close = _vfs_visitor_close;
    visitor->op.read = _vfs_visitor_read;
    visitor->op.write = _vfs_visitor_write;

    visitor->fh_gen = 1;
    vfs_map_init(&visitor->session_map, _vfs_map_cmp_session, NULL);
    vfs_rwlock_init(&visitor->session_map_lock);

    return &visitor->op;
}
