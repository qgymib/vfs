#include <stdlib.h>
#include <string.h>
#include "vfs/fs/nullfs.h"
#include "utils/atomic.h"
#include "utils/defs.h"
#include "utils/map.h"
#include "utils/mutex.h"

typedef struct vfs_nullfs_session
{
    ev_map_node_t       node;               /**< Map node. */

    vfs_atomic_t        refcnt;             /**< Reference count. */
    uintptr_t           fh;                 /**< File handle. */

    uint64_t            read_sz;            /**< Total bytes read. */
    uint64_t            write_sz;           /**< Total bytes written. */
    vfs_mutex_t         rw_lock;            /**< Read/Write lock. */
} vfs_nullfs_session_t;

typedef struct vfs_nullfs
{
    vfs_operations_t    op;                 /**< Base operations. */
    vfs_mutex_t         session_map_lock;   /**< Session map lock. */
    ev_map_t            session_map;        /**< Session map. */
} vfs_nullfs_t;

static void _vfs_nullfs_common_release_session(vfs_nullfs_session_t* session)
{
    if (vfs_atomic_dec(&session->refcnt) != 0)
    {
        return;
    }

    vfs_mutex_exit(&session->rw_lock);
    free(session);
}

static int _vfs_nullfs_common_fh(vfs_nullfs_t* fs, uintptr_t fh,
    int(*cb)(vfs_nullfs_session_t* session, void* data), void* data)
{
    int ret = 0;

    vfs_nullfs_session_t tmp_session;
    tmp_session.fh = fh;

    vfs_nullfs_session_t* session = NULL;
    vfs_mutex_enter(&fs->session_map_lock);
    do {
        ev_map_node_t* it = vfs_map_find(&fs->session_map, &tmp_session.node);
        if (it == NULL)
        {
            break;
        }
        session = EV_CONTAINER_OF(it, vfs_nullfs_session_t, node);
        (void)vfs_atomic_add(&session->refcnt);
    } while (0);
    vfs_mutex_leave(&fs->session_map_lock);

    if (session == NULL)
    {
        return VFS_EBADF;
    }

    ret = cb(session, data);
    _vfs_nullfs_common_release_session(session);

    return ret;
}

//////////////////////////////////////////////////////////////////////////
// destroy
//////////////////////////////////////////////////////////////////////////

static void _vfs_nullfs_destroy(struct vfs_operations* thiz)
{
    vfs_nullfs_t* fs = EV_CONTAINER_OF(thiz, vfs_nullfs_t, op);

    ev_map_node_t* it;
    while ((it = vfs_map_begin(&fs->session_map)) != NULL)
    {
        vfs_nullfs_session_t* session = EV_CONTAINER_OF(it, vfs_nullfs_session_t, node);

        vfs_map_erase(&fs->session_map, it);
        _vfs_nullfs_common_release_session(session);
    }

    vfs_mutex_exit(&fs->session_map_lock);
    free(fs);
}

//////////////////////////////////////////////////////////////////////////
// ls
//////////////////////////////////////////////////////////////////////////

static int _vfs_nullfs_ls(struct vfs_operations* thiz, const char* path, vfs_ls_cb fn, void* data)
{
    (void)thiz; (void)fn; (void)data;

    if (strcmp(path, "/") == 0)
    {
        return 0;
    }

    return VFS_ENOENT;
}

//////////////////////////////////////////////////////////////////////////
// stat
//////////////////////////////////////////////////////////////////////////

static int _vfs_nullfs_stat(struct vfs_operations* thiz, const char* path, vfs_stat_t* info)
{
    (void)thiz; (void)path; (void)info;

    if (strcmp(path, "/") == 0)
    {
        info->st_mode = VFS_S_IFDIR;
        info->st_mtime = 0;
        info->st_size = 0;
        return 0;
    }

    return VFS_ENOENT;
}

//////////////////////////////////////////////////////////////////////////
// open
//////////////////////////////////////////////////////////////////////////

static int _vfs_nullfs_open(struct vfs_operations* thiz, uintptr_t* fh, const char* path, uint64_t flags)
{
    (void)path;

    vfs_nullfs_t* fs = EV_CONTAINER_OF(thiz, vfs_nullfs_t, op);
    if (!(flags & VFS_O_CREATE))
    {
        return VFS_ENOENT;
    }
    if (strcmp(path, "/") == 0)
    {
        return VFS_EISDIR;
    }

    vfs_nullfs_session_t* session = calloc(1, sizeof(vfs_nullfs_session_t));
    if (session == NULL)
    {
        return VFS_ENOMEM;
    }

    session->refcnt = 1;
    session->fh = (uintptr_t)session;
    vfs_mutex_init(&session->rw_lock);

    ev_map_node_t* orig = NULL;
    vfs_mutex_enter(&fs->session_map_lock);
    {
        orig = vfs_map_insert(&fs->session_map, &session->node);
    }
    vfs_mutex_leave(&fs->session_map_lock);

    if (orig != NULL)
    {
        abort();
    }

    *fh = session->fh;
    return 0;
}

//////////////////////////////////////////////////////////////////////////
// close
//////////////////////////////////////////////////////////////////////////

static int _vfs_nullfs_close(struct vfs_operations* thiz, uintptr_t fh)
{
    vfs_nullfs_t* fs = EV_CONTAINER_OF(thiz, vfs_nullfs_t, op);

    vfs_nullfs_session_t tmp_session;
    tmp_session.fh = fh;

    vfs_nullfs_session_t* session = NULL;
    vfs_mutex_enter(&fs->session_map_lock);
    do {
        ev_map_node_t* it = vfs_map_find(&fs->session_map, &tmp_session.node);
        if (it == NULL)
        {
            break;
        }
        session = EV_CONTAINER_OF(it, vfs_nullfs_session_t, node);
        vfs_map_erase(&fs->session_map, it);
    } while (0);
    vfs_mutex_leave(&fs->session_map_lock);

    if (session == NULL)
    {
        return VFS_EBADF;
    }

    _vfs_nullfs_common_release_session(session);
    return 0;
}

//////////////////////////////////////////////////////////////////////////
// truncate
//////////////////////////////////////////////////////////////////////////

typedef struct vfs_nullfs_truncate_helper
{
    uint64_t    size;
} vfs_nullfs_truncate_helper_t;

static int _vfs_nullfs_truncate_inner(vfs_nullfs_session_t* session, void* data)
{
    vfs_nullfs_truncate_helper_t* helper = data;
    (void)session; (void)helper;

    return 0;
}

static int _vfs_nullfs_truncate(struct vfs_operations* thiz, uintptr_t fh, uint64_t size)
{
    vfs_nullfs_t* fs = EV_CONTAINER_OF(thiz, vfs_nullfs_t, op);
    vfs_nullfs_truncate_helper_t helper = { size };
    return _vfs_nullfs_common_fh(fs, fh, _vfs_nullfs_truncate_inner, &helper);
}

//////////////////////////////////////////////////////////////////////////
// seek
//////////////////////////////////////////////////////////////////////////

typedef struct vfs_nullfs_seek_helper
{
    int64_t offset;
} vfs_nullfs_seek_helper_t;

static int _vfs_nullfs_seek_inner(vfs_nullfs_session_t* session, void* data)
{
    (void)session; (void)data;
    return 0;
}

static int64_t _vfs_nullfs_seek(struct vfs_operations* thiz, uintptr_t fh, int64_t offset, int whence)
{
    (void)offset; (void)whence;
    vfs_nullfs_t* fs = EV_CONTAINER_OF(thiz, vfs_nullfs_t, op);

    return _vfs_nullfs_common_fh(fs, fh, _vfs_nullfs_seek_inner, NULL);
}

//////////////////////////////////////////////////////////////////////////
// read
//////////////////////////////////////////////////////////////////////////

typedef struct vfs_nullfs_read_helper
{
    void*   buf;
    size_t  len;
} vfs_nullfs_read_helper_t;

static int _vfs_nullfs_read_inner(vfs_nullfs_session_t* session, void* data)
{
    vfs_nullfs_read_helper_t* helper = data;
    memset(helper->buf, 0, helper->len);

    vfs_mutex_enter(&session->rw_lock);
    {
        session->read_sz += helper->len;
    }
    vfs_mutex_leave(&session->rw_lock);

    return 0;
}

static int _vfs_nullfs_read(struct vfs_operations* thiz, uintptr_t fh, void* buf, size_t len)
{
    int ret;
    vfs_nullfs_t* fs = EV_CONTAINER_OF(thiz, vfs_nullfs_t, op);

    vfs_nullfs_read_helper_t helper = { buf, len };
    if ((ret = _vfs_nullfs_common_fh(fs, fh, _vfs_nullfs_read_inner, &helper)) != 0)
    {
        return ret;
    }

    return (int)len;
}

//////////////////////////////////////////////////////////////////////////
// write
//////////////////////////////////////////////////////////////////////////

typedef struct vfs_nullfs_write_helper
{
    size_t  len;
} vfs_nullfs_write_helper_t;

static int _vfs_nullfs_write_inner(vfs_nullfs_session_t* session, void* data)
{
    vfs_nullfs_write_helper_t* helper = data;

    vfs_mutex_enter(&session->rw_lock);
    {
        session->write_sz += helper->len;
    }
    vfs_mutex_leave(&session->rw_lock);
    return 0;
}

static int _vfs_nullfs_write(struct vfs_operations* thiz, uintptr_t fh, const void* buf, size_t len)
{
    (void)buf;

    int ret;
    vfs_nullfs_t* fs = EV_CONTAINER_OF(thiz, vfs_nullfs_t, op);

    vfs_nullfs_write_helper_t helper = { len };
    if ((ret = _vfs_nullfs_common_fh(fs, fh, _vfs_nullfs_write_inner, &helper)) != 0)
    {
        return ret;
    }

    return (int)len;
}

//////////////////////////////////////////////////////////////////////////
// mkdir
//////////////////////////////////////////////////////////////////////////

static int _vfs_nullfs_mkdir(struct vfs_operations* thiz, const char* path)
{
    (void)thiz; (void)path;
    return 0;
}

//////////////////////////////////////////////////////////////////////////
// rmdir
//////////////////////////////////////////////////////////////////////////

static int _vfs_nullfs_rmdir(struct vfs_operations* thiz, const char* path)
{
    (void)thiz;

    if (strcmp(path, "/") == 0)
    {
        return VFS_EACCES;
    }

    return VFS_ENOENT;
}

//////////////////////////////////////////////////////////////////////////
// unlink
//////////////////////////////////////////////////////////////////////////

static int _vfs_nullfs_unlink(struct vfs_operations* thiz, const char* path)
{
    (void)thiz;

    if (strcmp(path, "/") == 0)
    {
        return VFS_EISDIR;
    }

    return VFS_ENOENT;
}

//////////////////////////////////////////////////////////////////////////
// API
//////////////////////////////////////////////////////////////////////////

static int _vfs_nullfs_cmp_session(const ev_map_node_t* key1, const ev_map_node_t* key2, void* arg)
{
    (void)arg;
    vfs_nullfs_session_t* session_1 = EV_CONTAINER_OF(key1, vfs_nullfs_session_t, node);
    vfs_nullfs_session_t* session_2 = EV_CONTAINER_OF(key2, vfs_nullfs_session_t, node);

    if (session_1->fh == session_2->fh)
    {
        return 0;
    }
    return session_1->fh < session_2->fh ? -1 : 1;
}

int vfs_make_null(vfs_operations_t** fs)
{
    vfs_nullfs_t* nullfs = calloc(1, sizeof(vfs_nullfs_t));
    if (nullfs == NULL)
    {
        return VFS_ENOMEM;
    }

    nullfs->op.destroy = _vfs_nullfs_destroy;
    nullfs->op.ls = _vfs_nullfs_ls;
    nullfs->op.stat = _vfs_nullfs_stat;
    nullfs->op.open = _vfs_nullfs_open;
    nullfs->op.close = _vfs_nullfs_close;
    nullfs->op.truncate = _vfs_nullfs_truncate;
    nullfs->op.seek = _vfs_nullfs_seek;
    nullfs->op.read = _vfs_nullfs_read;
    nullfs->op.write = _vfs_nullfs_write;
    nullfs->op.mkdir = _vfs_nullfs_mkdir;
    nullfs->op.rmdir = _vfs_nullfs_rmdir;
    nullfs->op.unlink = _vfs_nullfs_unlink;

    vfs_map_init(&nullfs->session_map, _vfs_nullfs_cmp_session, NULL);
    vfs_mutex_init(&nullfs->session_map_lock);

    *fs = &nullfs->op;
    return 0;
}
