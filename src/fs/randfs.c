/**
 * Required by #rand_s() in windows.
 * @see https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/rand-s
 */
#define _CRT_RAND_S
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "vfs/fs/randfs.h"
#include "utils/atomic.h"
#include "utils/defs.h"
#include "utils/errcode.h"
#include "utils/map.h"
#include "utils/mutex.h"

typedef struct vfs_randfs_session
{
    ev_map_node_t           node;
    vfs_atomic_t            refcnt;

    uintptr_t               fake;
    uint64_t                flags;
} vfs_randfs_session_t;

typedef struct vfs_randfs
{
    vfs_operations_t        op;                 /**< Base operations. */

    ev_map_t                session_map;        /**< Open file session map. */
    vfs_mutex_t             session_map_lock;
} vfs_randfs_t;

static void _vfs_randfs_acquire_session(vfs_randfs_session_t* session)
{
    (void)vfs_atomic_add(&session->refcnt);
}

static void _vfs_randfs_release_session(vfs_randfs_session_t* session)
{
    if (vfs_atomic_dec(&session->refcnt) != 0)
    {
        return;
    }

    free(session);
}

static void _vfs_randfs_cleanup_session(vfs_randfs_t* fs)
{
    ev_map_node_t* it;

    vfs_mutex_enter(&fs->session_map_lock);
    while ((it = vfs_map_begin(&fs->session_map)) != NULL)
    {
        vfs_randfs_session_t* session = EV_CONTAINER_OF(it, vfs_randfs_session_t, node);
        vfs_map_erase(&fs->session_map, it);

        vfs_mutex_leave(&fs->session_map_lock);
        {
            _vfs_randfs_release_session(session);
        }
        vfs_mutex_enter(&fs->session_map_lock);
    }
    vfs_mutex_leave(&fs->session_map_lock);
}

static int _vfs_randfs_cmp_session(const ev_map_node_t* key1, const ev_map_node_t* key2, void* arg)
{
    (void)arg;
    vfs_randfs_session_t* session_1 = EV_CONTAINER_OF(key1, vfs_randfs_session_t, node);
    vfs_randfs_session_t* session_2 = EV_CONTAINER_OF(key2, vfs_randfs_session_t, node);
    if (session_1->fake == session_2->fake)
    {
        return 0;
    }
    return session_1->fake < session_2->fake ? -1 : 1;
}

static int _vfs_randfs_op_fh(vfs_randfs_t* fs, uintptr_t fh,
    int (*cb)(vfs_randfs_session_t* session, void* data), void* data)
{
    vfs_randfs_session_t tmp;
    tmp.fake = fh;

    vfs_randfs_session_t* session = NULL;
    vfs_mutex_enter(&fs->session_map_lock);
    do
    {
        ev_map_node_t* it = vfs_map_find(&fs->session_map, &tmp.node);
        if (it == NULL)
        {
            break;
        }
        session = EV_CONTAINER_OF(it, vfs_randfs_session_t, node);
        _vfs_randfs_acquire_session(session);
    } while (0);
    vfs_mutex_leave(&fs->session_map_lock);

    if (session == NULL)
    {
        return VFS_EBADF;
    }

    int ret = cb(session, data);
    _vfs_randfs_release_session(session);

    return ret;
}

//////////////////////////////////////////////////////////////////////////
// destroy
//////////////////////////////////////////////////////////////////////////

static void _vfs_randfs_destroy(struct vfs_operations* thiz)
{
    vfs_randfs_t* fs = EV_CONTAINER_OF(thiz, vfs_randfs_t, op);

    _vfs_randfs_cleanup_session(fs);
    vfs_mutex_exit(&fs->session_map_lock);
    free(fs);
}

//////////////////////////////////////////////////////////////////////////
// stat
//////////////////////////////////////////////////////////////////////////

static int _vfs_randfs_stat(struct vfs_operations* thiz, const char* path, vfs_stat_t* info)
{
    vfs_randfs_t* fs = EV_CONTAINER_OF(thiz, vfs_randfs_t, op); (void)fs;

    if (strcmp(path, "/") == 0)
    {
        info->st_mode = VFS_S_IFDIR;
        info->st_mtime = 0;
        info->st_size = 0;
        return 0;
    }

    if (strcmp(path, "/random") == 0)
    {
        info->st_mode = VFS_S_IFREG;
        info->st_mtime = 0;
        info->st_size = 1;
        return 0;
    }

    return VFS_ENOENT;
}

//////////////////////////////////////////////////////////////////////////
// ls
//////////////////////////////////////////////////////////////////////////

static int _vfs_randfs_ls(struct vfs_operations* thiz, const char* path, vfs_ls_cb fn, void* data)
{
    int ret;
    if (strcmp(path, "/") != 0)
    {
        return VFS_ENOENT;
    }

    vfs_stat_t info;
    if ((ret = _vfs_randfs_stat(thiz, "/random", &info)) != 0)
    {
        return ret;
    }

    fn("random", &info, data);
    return 0;
}

//////////////////////////////////////////////////////////////////////////
// open
//////////////////////////////////////////////////////////////////////////

static int _vfs_randfs_open(struct vfs_operations* thiz, uintptr_t* fh, const char* path, uint64_t flags)
{
    vfs_randfs_t* fs = EV_CONTAINER_OF(thiz, vfs_randfs_t, op);

    if (strcmp(path, "/random") != 0)
    {
        return VFS_ENOENT;
    }

    vfs_randfs_session_t* session = malloc(sizeof(vfs_randfs_session_t));
    if (session == NULL)
    {
        return VFS_ENOMEM;
    }
    session->fake = (uintptr_t)session;
    session->flags = flags;
    session->refcnt = 1;

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

    *fh = session->fake;
    return 0;
}

//////////////////////////////////////////////////////////////////////////
// close
//////////////////////////////////////////////////////////////////////////

static int _vfs_randfs_close(struct vfs_operations* thiz, uintptr_t fh)
{
    vfs_randfs_t* fs = EV_CONTAINER_OF(thiz, vfs_randfs_t, op);

    vfs_randfs_session_t tmp;
    tmp.fake = fh;

    vfs_randfs_session_t* session = NULL;
    vfs_mutex_enter(&fs->session_map_lock);
    do 
    {
        ev_map_node_t* it = vfs_map_find(&fs->session_map, &tmp.node);
        if (it == NULL)
        {
            break;
        }
        session = EV_CONTAINER_OF(it, vfs_randfs_session_t, node);
        vfs_map_erase(&fs->session_map, it);
    } while (0);
    vfs_mutex_leave(&fs->session_map_lock);

    if (session == NULL)
    {
        return VFS_EBADF;
    }

    _vfs_randfs_release_session(session);
    return 0;
}

//////////////////////////////////////////////////////////////////////////
// truncate
//////////////////////////////////////////////////////////////////////////

static int _vfs_randfs_truncate_inner(vfs_randfs_session_t* session, void* data)
{
    (void)session; (void)data;
    return VFS_EINVAL;
}

static int _vfs_randfs_truncate(struct vfs_operations* thiz, uintptr_t fh, uint64_t size)
{
    (void)size;
    vfs_randfs_t* fs = EV_CONTAINER_OF(thiz, vfs_randfs_t, op);
    return _vfs_randfs_op_fh(fs, fh, _vfs_randfs_truncate_inner, NULL);
}

//////////////////////////////////////////////////////////////////////////
// seek
//////////////////////////////////////////////////////////////////////////

static int _vfs_randfs_seek_inner(vfs_randfs_session_t* session, void* data)
{
    (void)session; (void)data;
    return VFS_ESPIPE;
}

static int64_t _vfs_randfs_seek(struct vfs_operations* thiz, uintptr_t fh, int64_t offset, int whence)
{
    (void)offset; (void)whence;
    vfs_randfs_t* fs = EV_CONTAINER_OF(thiz, vfs_randfs_t, op);
    return _vfs_randfs_op_fh(fs, fh, _vfs_randfs_seek_inner, NULL);
}

//////////////////////////////////////////////////////////////////////////
// read
//////////////////////////////////////////////////////////////////////////

typedef struct vfs_randfs_read_helper
{
    void*   buf;
    size_t  len;
} vfs_randfs_read_helper_t;

#if defined(_WIN32)

static int _vfs_randfs_getrandom(uint8_t* buf, size_t buflen)
{
    unsigned int randval;
    if (rand_s(&randval) != 0)
    {
        int errcode = errno;
        errcode = vfs_translate_posix_error(errcode);
        return errcode;
    }

    size_t copy_sz = min(sizeof(randval), buflen);
    memcpy(buf, &randval, copy_sz);

    return (int)copy_sz;
}

#else

#include <sys/random.h>

static int _vfs_randfs_getrandom(uint8_t* buf, size_t buflen)
{
    return getrandom(buf, buflen, 0);
}

#endif

static int _vfs_randfs_read_inner(vfs_randfs_session_t* session, void* data)
{
    (void)session;

    vfs_randfs_read_helper_t* helper = data;
    uint8_t* buf = helper->buf;
    size_t len = helper->len;

    while (len > 0)
    {
        int read_sz = _vfs_randfs_getrandom(buf, len);
        if (read_sz < 0)
        {
            int errcode = errno;
            if (errcode == EINTR)
            {
                continue;
            }
            errcode = vfs_translate_posix_error(errcode);
            return errcode;
        }

        len -= read_sz;
        buf += read_sz;
    }
    
    return (int)helper->len;
}

static int _vfs_randfs_read(struct vfs_operations* thiz, uintptr_t fh, void* buf, size_t len)
{
    vfs_randfs_t* fs = EV_CONTAINER_OF(thiz, vfs_randfs_t, op);
    vfs_randfs_read_helper_t helper = { buf, len };
    return _vfs_randfs_op_fh(fs, fh, _vfs_randfs_read_inner, &helper);
}

//////////////////////////////////////////////////////////////////////////
// write
//////////////////////////////////////////////////////////////////////////

typedef struct vfs_randfs_write_helper
{
    const void* buf;
    size_t      len;
} vfs_randfs_write_helper_t;

static int _vfs_randfs_write_inner(vfs_randfs_session_t* session, void* data)
{
    (void)session;
    vfs_randfs_write_helper_t* helper = data;
    return (int)helper->len;
}

static int _vfs_randfs_write(struct vfs_operations* thiz, uintptr_t fh, const void* buf, size_t len)
{
    vfs_randfs_t* fs = EV_CONTAINER_OF(thiz, vfs_randfs_t, op);
    vfs_randfs_write_helper_t helper = { buf, len };
    return _vfs_randfs_op_fh(fs, fh, _vfs_randfs_write_inner, &helper);
}

//////////////////////////////////////////////////////////////////////////
// API
//////////////////////////////////////////////////////////////////////////

int vfs_make_random(vfs_operations_t** fs)
{
    vfs_randfs_t* randfs = calloc(1, sizeof(vfs_randfs_t));
    if (randfs == NULL)
    {
        return VFS_ENOMEM;
    }

    randfs->op.destroy = _vfs_randfs_destroy;
    randfs->op.stat = _vfs_randfs_stat;
    randfs->op.ls = _vfs_randfs_ls;
    randfs->op.open = _vfs_randfs_open;
    randfs->op.close = _vfs_randfs_close;
    randfs->op.truncate = _vfs_randfs_truncate;
    randfs->op.seek = _vfs_randfs_seek;
    randfs->op.read = _vfs_randfs_read;
    randfs->op.write = _vfs_randfs_write;

    vfs_map_init(&randfs->session_map, _vfs_randfs_cmp_session, NULL);
    vfs_mutex_init(&randfs->session_map_lock);

    *fs = &randfs->op;
    return 0;
}
