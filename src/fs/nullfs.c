#include <stdlib.h>
#include <string.h>
#include "vfs/fs/nullfs.h"
#include "memfs.h"
#include "utils/defs.h"

typedef struct vfs_nullfs
{
    vfs_operations_t    op;         /**< Base operations. */
    vfs_operations_t*   memfs;      /**< Underlying file system. */
} vfs_nullfs_t;

//////////////////////////////////////////////////////////////////////////
// destroy
//////////////////////////////////////////////////////////////////////////

static void _vfs_nullfs_destroy(struct vfs_operations* thiz)
{
    vfs_nullfs_t* fs = EV_CONTAINER_OF(thiz, vfs_nullfs_t, op);

    if (fs->memfs != NULL)
    {
        fs->memfs->destroy(fs->memfs);
        fs->memfs = NULL;
    }

    free(fs);
}

//////////////////////////////////////////////////////////////////////////
// ls
//////////////////////////////////////////////////////////////////////////

static int _vfs_nullfs_ls(struct vfs_operations* thiz, const char* path, vfs_ls_cb fn, void* data)
{
    vfs_nullfs_t* fs = EV_CONTAINER_OF(thiz, vfs_nullfs_t, op);
    vfs_operations_t* memfs = fs->memfs;
    return memfs->ls(memfs, path, fn, data);
}

//////////////////////////////////////////////////////////////////////////
// stat
//////////////////////////////////////////////////////////////////////////

static int _vfs_nullfs_stat(struct vfs_operations* thiz, const char* path, vfs_stat_t* info)
{
    vfs_nullfs_t* fs = EV_CONTAINER_OF(thiz, vfs_nullfs_t, op);
    vfs_operations_t* memfs = fs->memfs;
    return memfs->stat(memfs, path, info);
}

//////////////////////////////////////////////////////////////////////////
// open
//////////////////////////////////////////////////////////////////////////

static int _vfs_nullfs_open(struct vfs_operations* thiz, uintptr_t* fh, const char* path, uint64_t flags)
{
    vfs_nullfs_t* fs = EV_CONTAINER_OF(thiz, vfs_nullfs_t, op);
    vfs_operations_t* memfs = fs->memfs;
    return memfs->open(memfs, fh, path, flags);
}

//////////////////////////////////////////////////////////////////////////
// close
//////////////////////////////////////////////////////////////////////////

static int _vfs_nullfs_close(struct vfs_operations* thiz, uintptr_t fh)
{
    vfs_nullfs_t* fs = EV_CONTAINER_OF(thiz, vfs_nullfs_t, op);
    vfs_operations_t* memfs = fs->memfs;
    return memfs->close(memfs, fh);
}

//////////////////////////////////////////////////////////////////////////
// truncate
//////////////////////////////////////////////////////////////////////////

static int _vfs_nullfs_truncate(struct vfs_operations* thiz, uintptr_t fh, uint64_t size)
{
    vfs_nullfs_t* fs = EV_CONTAINER_OF(thiz, vfs_nullfs_t, op);
    vfs_operations_t* memfs = fs->memfs;
    return memfs->truncate(memfs, fh, size);
}

//////////////////////////////////////////////////////////////////////////
// seek
//////////////////////////////////////////////////////////////////////////

static int64_t _vfs_nullfs_seek(struct vfs_operations* thiz, uintptr_t fh, int64_t offset, int whence)
{
    vfs_nullfs_t* fs = EV_CONTAINER_OF(thiz, vfs_nullfs_t, op);
    vfs_operations_t* memfs = fs->memfs;
    return memfs->seek(memfs, fh, offset, whence);
}

//////////////////////////////////////////////////////////////////////////
// read
//////////////////////////////////////////////////////////////////////////

static int _vfs_nullfs_read_hook(vfs_memfs_session_t* session, void* buf, size_t len, void* data)
{
    (void)data;

    memset(buf, 0, len);

    if (session->data.fpos != UINT64_MAX)
    {
        session->data.fpos += len;
    }

    return (int)len;
}

static int _vfs_nullfs_read(struct vfs_operations* thiz, uintptr_t fh, void* buf, size_t len)
{
    vfs_nullfs_t* fs = EV_CONTAINER_OF(thiz, vfs_nullfs_t, op);
    vfs_operations_t* memfs = fs->memfs;
    return memfs->read(memfs, fh, buf, len);
}

//////////////////////////////////////////////////////////////////////////
// write
//////////////////////////////////////////////////////////////////////////

static int _vfs_nullfs_write_hook(vfs_memfs_session_t* session, const void* buf, size_t len, void* data)
{
    (void)buf; (void)data;

    vfs_memfs_node_t* node = session->data.node;

    if (session->data.fpos != UINT64_MAX)
    {
        session->data.fpos += len;
    }

    node->stat.st_size += len;

    return (int)len;
}

static int _vfs_nullfs_write(struct vfs_operations* thiz, uintptr_t fh, const void* buf, size_t len)
{
    vfs_nullfs_t* fs = EV_CONTAINER_OF(thiz, vfs_nullfs_t, op);
    vfs_operations_t* memfs = fs->memfs;
    return memfs->write(memfs, fh, buf, len);
}

//////////////////////////////////////////////////////////////////////////
// mkdir
//////////////////////////////////////////////////////////////////////////

static int _vfs_nullfs_mkdir(struct vfs_operations* thiz, const char* path)
{
    vfs_nullfs_t* fs = EV_CONTAINER_OF(thiz, vfs_nullfs_t, op);
    vfs_operations_t* memfs = fs->memfs;
    return memfs->mkdir(memfs, path);
}

//////////////////////////////////////////////////////////////////////////
// rmdir
//////////////////////////////////////////////////////////////////////////

static int _vfs_nullfs_rmdir(struct vfs_operations* thiz, const char* path)
{
    vfs_nullfs_t* fs = EV_CONTAINER_OF(thiz, vfs_nullfs_t, op);
    vfs_operations_t* memfs = fs->memfs;
    return memfs->rmdir(memfs, path);
}

//////////////////////////////////////////////////////////////////////////
// unlink
//////////////////////////////////////////////////////////////////////////

static int _vfs_nullfs_unlink(struct vfs_operations* thiz, const char* path)
{
    vfs_nullfs_t* fs = EV_CONTAINER_OF(thiz, vfs_nullfs_t, op);
    vfs_operations_t* memfs = fs->memfs;
    return memfs->unlink(memfs, path);
}

int vfs_make_null(vfs_operations_t** fs)
{
    int ret;
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

    if ((ret = vfs_make_memory(&nullfs->memfs)) != 0)
    {
        _vfs_nullfs_destroy(&nullfs->op);
        return ret;
    }

    const vfs_memfs_io_t io = {
        NULL,
        _vfs_nullfs_read_hook,
        _vfs_nullfs_write_hook,
    };
    vfs_memfs_set_io(nullfs->memfs, &io);

    *fs = &nullfs->op;
    return 0;
}
