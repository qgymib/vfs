#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "vfs/fs/overlayfs.h"
#include "utils/atomic.h"
#include "utils/defs.h"
#include "utils/strlist.h"
#include "utils/dir.h"
#include "utils/map.h"
#include "utils/mutex.h"

/**
 * @brief Suffix for whiteout files or directories.
 *
 * A whiteout file (directory) is a special file (directory) in the upper file
 * system that indicate that the file (directory) is deleted in the lower file
 * system. A whiteout file cannot exist if the targeting file also exists in the
 * upper layer.
 *
 * For example, the whiteout file `/foo/bar.whiteout` in the upper file system
 * means `/foo/bar` should be marked as deleted in the lower file system. There
 * can not be a file `/foo/bar` in the upper file system.
 *
 * The type of whiteout file (directory) does not matter.
 *
 * If a file (directory) exist in overlay file system, it means:
 * 1. It either exist in the upper layer or in the lower layer.
 * 2. There is no whiteout file for this entry.
 */
#define OVERLAY_WHITEOUT_SUFFIX     ".whiteout"
#define OVERLAY_WHITEOUT_SUFFIX_SZ  (sizeof(OVERLAY_WHITEOUT_SUFFIX) - 1)

typedef enum vfs_overlayfs_stat_ret
{
    VFS_OVERLAYFS_STAT_NOENT    = VFS_ENOENT,
    VFS_OVERLAYFS_STAT_WHITEOUT = -1,
    VFS_OVERLAYFS_STAT_LOWER    = 0,
    VFS_OVERLAYFS_STAT_UPPER    = 1,
} vfs_overlayfs_stat_ret_t;

typedef struct vfs_overlayfs_stat_helper
{
    vfs_str_t*          basename;
    int                 found;
    vfs_stat_t*         info;
} vfs_overlayfs_stat_helper_t;

typedef struct vfs_overlayfs_session
{
    ev_map_node_t       node;       /**< Map node. */
    vfs_atomic_t        refcnt;     /**< Reference count. */

    vfs_operations_t*   fs;         /**< The file system working on. */
    uintptr_t           fake;       /**< Fake file handle. */

    /**
     * @brief Real file handle.
     * This field is valid as long as #vfs_overlayfs_session_t::fs is not NULL.
     */
    uintptr_t           real;
} vfs_overlayfs_session_t;

typedef struct vfs_overlayfs
{
    vfs_operations_t    op;     /**< Base operations. */

    /**
     * @brief The lower file system.
     * It is always READ ONLY.
     */
    vfs_operations_t*   lower;

    /**
     * @brief The upper file system.
     * All modifications are done in the upper file system.
     */
    vfs_operations_t*   upper;

    /**
     * @brief All open session.
     * @see #vfs_overlayfs_session_t.
     */
    ev_map_t            session_map;

    /**
     * @brief Mutex for #vfs_overlayfs_t::session_map.
     */
    vfs_mutex_t         session_map_lock;
} vfs_overlayfs_t;

static int _vfs_overlayfs_cmp_session(const ev_map_node_t* key1, const ev_map_node_t* key2, void* arg)
{
    (void)arg;
    vfs_overlayfs_session_t* session_1 = EV_CONTAINER_OF(key1, vfs_overlayfs_session_t, node);
    vfs_overlayfs_session_t* session_2 = EV_CONTAINER_OF(key2, vfs_overlayfs_session_t, node);

    if (session_1->fake == session_2->fake)
    {
        return 0;
    }
    return session_1->fake < session_2->fake ? -1 : 1;
}

static int _vfs_overlayfs_common_remove_whiteout_entry(vfs_overlayfs_t* fs, const vfs_str_t* path)
{
    int ret;
    if (fs->upper->unlink == NULL || fs->upper->rmdir == NULL)
    {
        return VFS_ENOSYS;
    }

    if ((ret = fs->upper->unlink(fs->upper, path->str)) == 0)
    {/* unlink success. */
        return 0;
    }
    if (ret == VFS_ENOENT)
    {/* no such file. */
        return 0;
    }
    if (ret != VFS_EISDIR)
    {/* unknown error. */
        return ret;
    }

    if ((ret = fs->upper->rmdir(fs->upper, path->str)) == 0)
    {
        return 0;
    }
    if (ret == VFS_ENOENT)
    {
        return 0;
    }
    return ret;
}

static int _vfs_overlayfs_common_remove_whiteout(vfs_overlayfs_t* fs, const vfs_str_t* path)
{
    int ret;

    vfs_str_t whiteout = vfs_str_dup(path);
    vfs_str_append(&whiteout, OVERLAY_WHITEOUT_SUFFIX, OVERLAY_WHITEOUT_SUFFIX_SZ);
    {
        ret = _vfs_overlayfs_common_remove_whiteout_entry(fs, &whiteout);
    }
    vfs_str_exit(&whiteout);

    return ret;
}

static int _vfs_overlayfs_common_is_parent_exist(vfs_overlayfs_t* fs, const vfs_str_t* path)
{
    int ret;
    vfs_str_t parent = vfs_path_parent(path, NULL);
    do {
        vfs_stat_t info;
        if ((ret = fs->op.stat(&fs->op, parent.str, &info)) != 0)
        {
            break;
        }
        if (!(info.st_mode & VFS_S_IFDIR))
        {
            ret = VFS_ENOTDIR;
        }
    } while (0);
    vfs_str_exit(&parent);
    return ret;
}

static void _vfs_overlayfs_common_acquire_session(vfs_overlayfs_session_t* session)
{
    (void)vfs_atomic_add(&session->refcnt);
}

static void _vfs_overlayfs_common_release_session(vfs_overlayfs_session_t* session)
{
    if (vfs_atomic_dec(&session->refcnt) != 0)
    {
        return;
    }

    if (session->fs != NULL)
    {
        session->fs->close(session->fs, session->real);
        session->fs = NULL;
    }
    session->real = 0;
    free(session);
}

/**
 * @brief Find session by \p fh file handle. If found, refcnt is increased.
 * @param[in] fs - File system instance.
 * @param[in] fh - File handle.
 * @return Found session, or NULL if not found.
 */
static vfs_overlayfs_session_t* _vfs_overlayfs_common_find_session(vfs_overlayfs_t* fs, uintptr_t fh)
{
    vfs_overlayfs_session_t tmp_session;
    tmp_session.fake = fh;

    vfs_overlayfs_session_t* session = NULL;
    vfs_mutex_enter(&fs->session_map_lock);
    do 
    {
        ev_map_node_t* it = vfs_map_find(&fs->session_map, &tmp_session.node);
        if (it == NULL)
        {
            break;
        }
        session = EV_CONTAINER_OF(it, vfs_overlayfs_session_t, node);
        _vfs_overlayfs_common_acquire_session(session);
    } while (0);
    vfs_mutex_leave(&fs->session_map_lock);

    return session;
}

static int _vfs_overlayfs_common_stat_wrap_ls(const char* name, const vfs_stat_t* stat, void* data)
{
    vfs_overlayfs_stat_helper_t* helper = data;

    if (strcmp(helper->basename->str, name) != 0)
    {
        return 0;
    }

    helper->found = 1;
    memcpy(helper->info, stat, sizeof(*stat));
    return 1;
}

/**
 * @brief Wrapper for #vfs_operations_t::stat().
 *
 * The upper layer and lower layer might not implement #vfs_operations_t::stat()
 * but does have #vfs_operations_t::ls(), so in that case we use ls() if stat()
 * is not implemented.
 */
static int _vfs_overlayfs_common_stat_wrap(vfs_operations_t* fs, const vfs_str_t* path, vfs_stat_t* info)
{
    int ret;

    /* Try use #vfs_operations_t::stat() if possible. */
    if (fs->stat != NULL)
    {
        return fs->stat(fs, path->str, info);
    }

    /* Then try #vfs_operations_t::ls(). */
    if (fs->ls == NULL)
    {
        return VFS_ENOSYS;
    }

    /* Get parent directory. */
    vfs_str_t basename = VFS_STR_INIT;
    vfs_str_t parent = vfs_path_parent(path, &basename);
    if (parent.len == 0)
    {
        vfs_str_reset(&parent);
        vfs_str_append2(&parent, path);
    }

    vfs_overlayfs_stat_helper_t helper = { &basename, 0, info };
    ret = fs->ls(fs, parent.str, _vfs_overlayfs_common_stat_wrap_ls, &helper);
    vfs_str_exit(&basename);
    vfs_str_exit(&parent);

    if (ret != 0)
    {
        return ret;
    }
    if (!helper.found)
    {
        return VFS_ENOENT;
    }
    return 0;
}

/**
 * @brief Extended stat function for overlayfs.
 * @see #vfs_overlayfs_stat_ret_t.
 * @param[in] fs - The file system working on.
 * @param[in] path - Path to the file. Encoding in UTF-8.
 * @param[out] info - File stat.
 * @param[out] whiteout - The whiteout file name. It is only set when return value is #VFS_OVERLAYFS_STAT_WHITEOUT.
 * @return - VFS_OVERLAYFS_STAT_UPPER: The file exists in the upper file system.
 * @return - VFS_OVERLAYFS_STAT_LOWER: The file exists in the lower file system.
 * @return - VFS_OVERLAYFS_STAT_WHITEOUT: The file exist in the lower file system but whiteout in upper file system.
 * @return - VFS_OVERLAYFS_STAT_NOENT: The file does not exist in any file system.
 */
static int _vfs_overlayfs_common_stat_ex(vfs_overlayfs_t* fs, const vfs_str_t* path,
    vfs_stat_t* info, vfs_str_t* whiteout)
{
    int ret;

    if ((ret = _vfs_overlayfs_common_stat_wrap(fs->upper, path, info)) == 0)
    {
        return VFS_OVERLAYFS_STAT_UPPER;
    }

    /*
     * For path like `/foo/bar`, we need to check whether `/foo.whiteout` and
     * `/foo/bar.whiteout` exist.
     */
    for (size_t i = 1; ; i++)
    {
        vfs_str_t layer = vfs_path_layer(path, i);
        if (VFS_STR_IS_EMPTY(&layer))
        {
            vfs_str_exit(&layer);
            break;
        }
        vfs_str_append(&layer, OVERLAY_WHITEOUT_SUFFIX, OVERLAY_WHITEOUT_SUFFIX_SZ);

        if ((ret = _vfs_overlayfs_common_stat_wrap(fs->upper, &layer, info)) == 0)
        {
            if (whiteout != NULL)
            {
                vfs_str_reset(whiteout);
                vfs_str_append2(whiteout, &layer);
            }
        }
        vfs_str_exit(&layer);

        /* Whiteout file/directory is exist, treat as deleted. */
        if (ret == 0)
        {
            return VFS_OVERLAYFS_STAT_WHITEOUT;
        }
    }

    /* Now we can access lower layer. */
    if ((ret = _vfs_overlayfs_common_stat_wrap(fs->lower, path, info)) == 0)
    {
        return VFS_OVERLAYFS_STAT_LOWER;
    }

    return VFS_OVERLAYFS_STAT_NOENT;
}

static int _vfs_overlayfs_common_fh(vfs_overlayfs_t* fs, uintptr_t fh,
    int (*cb)(vfs_overlayfs_session_t* session, void* data), void* data)
{
    int ret;
    vfs_overlayfs_session_t* session = _vfs_overlayfs_common_find_session(fs, fh);
    if (session == NULL)
    {
        return VFS_EBADF;
    }

    ret = cb(session, data);
    _vfs_overlayfs_common_release_session(session);

    return ret;
}

//////////////////////////////////////////////////////////////////////////
// destroy
//////////////////////////////////////////////////////////////////////////

static void _vfs_overlayfs_destroy_cleanup(vfs_overlayfs_t* fs)
{
    ev_map_node_t* it;

    vfs_mutex_enter(&fs->session_map_lock);
    while ((it = vfs_map_begin(&fs->session_map)) != NULL)
    {
        vfs_overlayfs_session_t* session = EV_CONTAINER_OF(it, vfs_overlayfs_session_t, node);
        vfs_map_erase(&fs->session_map, it);
        vfs_mutex_leave(&fs->session_map_lock);
        {
            _vfs_overlayfs_common_release_session(session);
        }
        vfs_mutex_enter(&fs->session_map_lock);
    }
    vfs_mutex_leave(&fs->session_map_lock);
}

static void _vfs_overlayfs_destroy(struct vfs_operations* thiz)
{
    vfs_overlayfs_t* fs = EV_CONTAINER_OF(thiz, vfs_overlayfs_t, op);

    _vfs_overlayfs_destroy_cleanup(fs);

    if (fs->lower != NULL)
    {
        fs->lower->destroy(fs->lower);
        fs->lower = NULL;
    }

    if (fs->upper != NULL)
    {
        fs->upper->destroy(fs->upper);
        fs->upper = NULL;
    }

    vfs_mutex_exit(&fs->session_map_lock);
    free(fs);
}

//////////////////////////////////////////////////////////////////////////
// ls
//////////////////////////////////////////////////////////////////////////

typedef enum vfs_overlayfs_type
{
    /**
     * @brief Item only exist in lower file system.
     */
    VFS_OVERLAY_LOWER       = 0x01,

    /**
     * @brief Item only exist in upper file system.
     */
    VFS_OVERLAY_UPPER       = 0x02,

    /**
     * @brief Item exist in lower file system, but override by upper file system.
     */
    VFS_OVERLAY_OVERRIDE    = VFS_OVERLAY_LOWER | VFS_OVERLAY_UPPER,

    /**
     * @brief Item exist in lower file system, but whiteout in upper file system.
     */
    VFS_OVERLAY_WHITEOUT    = VFS_OVERLAY_LOWER | 0x04,
    
} vfs_overlayfs_type_t;

typedef struct vfs_overlayfs_item
{
    ev_map_node_t           node;
    vfs_str_t               name;
    vfs_stat_t              info;
    vfs_overlayfs_type_t    type;
} vfs_overlayfs_item_t;

typedef struct vfs_overlayfs_ls_helper
{
    ev_map_t*       item_map;
    int             ret;
} vfs_overlayfs_ls_helper_t;

static int _vfs_overlayfs_cmp_item(const ev_map_node_t* key1, const ev_map_node_t* key2,
    void* arg)
{
    (void)arg;
    vfs_overlayfs_item_t* item_1 = EV_CONTAINER_OF(key1, vfs_overlayfs_item_t, node);
    vfs_overlayfs_item_t* item_2 = EV_CONTAINER_OF(key2, vfs_overlayfs_item_t, node);
    return vfs_str_cmp2(&item_1->name, &item_2->name);
}

static void _vfs_overlayfs_ls_destroy_item(vfs_overlayfs_item_t* item)
{
    vfs_str_exit(&item->name);
    free(item);
}

static int _vfs_overlayfs_ls_append_to_map(vfs_overlayfs_ls_helper_t* helper,
    const char* name, const vfs_stat_t* stat, int type)
{
    ev_map_node_t* orig;
    vfs_overlayfs_item_t* item = malloc(sizeof(vfs_overlayfs_item_t));
    if (item == NULL)
    {
        helper->ret = VFS_ENOMEM;
        return 1;
    }
    item->name = vfs_str_from1(name);
    item->info = *stat;
    item->type = type;

    if ((orig = vfs_map_insert(helper->item_map, &item->node)) == NULL)
    {
        return 0;
    }

    vfs_overlayfs_item_t* orig_item = EV_CONTAINER_OF(orig, vfs_overlayfs_item_t, node);
    assert(orig_item->type == VFS_OVERLAY_LOWER);
    assert(type == VFS_OVERLAY_UPPER);
    item->type |= orig_item->type;

    vfs_map_erase(helper->item_map, orig);
    _vfs_overlayfs_ls_destroy_item(orig_item);

    if ((orig = vfs_map_insert(helper->item_map, &item->node)) != NULL)
    {
        abort();
    }

    return 0;
}

static int _vfs_overlayfs_ls_on_lower(const char* name, const vfs_stat_t* stat, void* data)
{
    vfs_overlayfs_ls_helper_t* helper = data;
    return _vfs_overlayfs_ls_append_to_map(helper, name, stat, VFS_OVERLAY_LOWER);
}

static void _vfs_overlayfs_ls_cleanup(ev_map_t* item_map)
{
    ev_map_node_t* it;
    while ((it = vfs_map_begin(item_map)) != NULL)
    {
        vfs_overlayfs_item_t* item = EV_CONTAINER_OF(it, vfs_overlayfs_item_t, node);
        vfs_map_erase(item_map, it);
        _vfs_overlayfs_ls_destroy_item(item);
    }
}

static vfs_overlayfs_item_t* _fs_overlayfs_ls_search_item(ev_map_t* item_map, const vfs_str_t* name)
{
    vfs_overlayfs_item_t tmp_node;
    tmp_node.name = *name;

    ev_map_node_t* it = vfs_map_find(item_map, &tmp_node.node);
    if (it == NULL)
    {
        abort();
    }

    vfs_overlayfs_item_t* item = EV_CONTAINER_OF(it, vfs_overlayfs_item_t, node);
    return item;
}

static int _vfs_overlayfs_ls_on_upper(const char* name, const vfs_stat_t* stat, void* data)
{
    vfs_overlayfs_ls_helper_t* helper = data;
    vfs_str_t name_str = vfs_str_from_static1(name);

    /* If it is not a whiteout file, append the item in the records. */
    if (!vfs_str_endwith(&name_str, OVERLAY_WHITEOUT_SUFFIX, OVERLAY_WHITEOUT_SUFFIX_SZ))
    {
        return _vfs_overlayfs_ls_append_to_map(helper, name, stat, VFS_OVERLAY_UPPER);
    }

    /* Remove the record caused by the whiteout. */
    const size_t left_sz = name_str.len - OVERLAY_WHITEOUT_SUFFIX_SZ;
    vfs_str_t tmp_name = vfs_str_sub(&name_str, 0, left_sz);
    {
        vfs_overlayfs_item_t* item = _fs_overlayfs_ls_search_item(helper->item_map, &tmp_name);
        assert(item->type == VFS_OVERLAY_LOWER);
        item->type = VFS_OVERLAY_WHITEOUT;
    }
    vfs_str_exit(&tmp_name);
    return 0;
}

static void _vfs_overlayfs_ls_docb(ev_map_t* item_map, vfs_ls_cb fn, void* data)
{
    ev_map_node_t* it = vfs_map_begin(item_map);
    for (; it != NULL; it = vfs_map_next(it))
    {
        vfs_overlayfs_item_t* item = EV_CONTAINER_OF(it, vfs_overlayfs_item_t, node);

        if (item->type != VFS_OVERLAY_WHITEOUT)
        {
            fn(item->name.str, &item->info, data);
        }
    }
}

static int _vfs_overlayfs_ls_inner(vfs_overlayfs_t* fs, const vfs_str_t* path,
    vfs_ls_cb fn, void* data)
{
    int ret = 0;
    if (fs->lower->ls == NULL || fs->upper->ls == NULL)
    {
        return VFS_ENOSYS;
    }

    ev_map_t item_map;
    vfs_map_init(&item_map, _vfs_overlayfs_cmp_item, NULL);

    /* First get all items from lower layer. */
    vfs_overlayfs_ls_helper_t helper = { &item_map, 0 };
    ret = fs->lower->ls(fs->lower, path->str, _vfs_overlayfs_ls_on_lower, &helper);
    if (ret != 0 && ret != VFS_ENOENT)
    {
        goto finish;
    }
    if (helper.ret != 0)
    {
        ret = helper.ret;
        goto finish;
    }

    /* Fix items by check upper layer. */
    ret = fs->upper->ls(fs->upper, path->str, _vfs_overlayfs_ls_on_upper, &helper);
    if (ret != 0 && ret != VFS_ENOENT)
    {
        goto finish;
    }
    if (helper.ret != 0)
    {
        ret = helper.ret;
        goto finish;
    }

    _vfs_overlayfs_ls_docb(&item_map, fn, data);
    ret = 0;

finish:
    _vfs_overlayfs_ls_cleanup(&item_map);
    return ret;
}

static int _vfs_overlayfs_ls(struct vfs_operations* thiz, const char* path,
    vfs_ls_cb fn, void* data)
{
    int ret;
    vfs_overlayfs_t* fs = EV_CONTAINER_OF(thiz, vfs_overlayfs_t, op);
    
    vfs_stat_t info;
    if ((ret = thiz->stat(thiz, path, &info)) != 0)
    {
        return ret;
    }

    if (info.st_mode & VFS_S_IFREG)
    {
        return VFS_ENOTDIR;
    }

    vfs_str_t path_str = vfs_str_from_static1(path);
    return _vfs_overlayfs_ls_inner(fs, &path_str, fn, data);
}

//////////////////////////////////////////////////////////////////////////
// stat
//////////////////////////////////////////////////////////////////////////

static int _vfs_overlayfs_stat(struct vfs_operations* thiz, const char* path, vfs_stat_t* info)
{
    vfs_overlayfs_t* fs = EV_CONTAINER_OF(thiz, vfs_overlayfs_t, op);
    vfs_str_t path_str = vfs_str_from_static1(path);

    int ret = _vfs_overlayfs_common_stat_ex(fs, &path_str, info, NULL);
    switch (ret)
    {
    case VFS_OVERLAYFS_STAT_LOWER:
    case VFS_OVERLAYFS_STAT_UPPER:
        return 0;

    default:
        break;
    }

    return VFS_ENOENT;
}

//////////////////////////////////////////////////////////////////////////
// open
//////////////////////////////////////////////////////////////////////////

static int _vfs_overlayfs_open_with_fs(vfs_overlayfs_t* fs, vfs_operations_t* op,
    uintptr_t* fh, const vfs_str_t* path, uint64_t flags)
{
    int ret;
    vfs_overlayfs_session_t* session = malloc(sizeof(vfs_overlayfs_session_t));
    if (session == NULL)
    {
        return VFS_ENOMEM;
    }
    session->refcnt = 1;
    session->fs = NULL;
    session->fake = (uintptr_t)session;
    session->real = 0;

    /* Open file. */
    if ((ret = op->open(op, &session->real, path->str, flags)) != 0)
    {
        _vfs_overlayfs_common_release_session(session);
        return ret;
    }
    session->fs = op;

    /* Save session. */
    ev_map_node_t* orig;
    vfs_mutex_enter(&fs->session_map_lock);
    {
        orig = vfs_map_insert(&fs->session_map, &session->node);
    }
    vfs_mutex_leave(&fs->session_map_lock);
    
    if (orig != NULL)
    {/* Should not happen. */
        abort();
    }
    *fh = session->fake;

    return 0;
}

static int _vfs_overlayfs_open_copy_to_upper(vfs_overlayfs_t* fs, const vfs_str_t* path)
{
    int ret = 0;
    if (fs->lower->open == NULL || fs->upper->open == NULL
        || fs->lower->close == NULL || fs->upper->close == NULL)
    {
        return VFS_ENOSYS;
    }

    if ((ret = vfs_path_ensure_parent_exist(fs->upper, path)) != 0)
    {
        return ret;
    }

    uintptr_t fh_lower = 0;
    if ((ret = fs->lower->open(fs->lower, &fh_lower, path->str, VFS_O_RDONLY)) != 0)
    {
        return ret;
    }

    uintptr_t fh_upper = 0;
    if ((ret = fs->upper->open(fs->upper, &fh_upper, path->str,
        VFS_O_WRONLY | VFS_O_CREATE | VFS_O_TRUNCATE)) != 0)
    {
        fs->lower->close(fs->lower, fh_lower);
        return ret;
    }

    const size_t buf_sz = 64 * 1024;
    char* buf = malloc(buf_sz);
    if (buf == NULL)
    {
        fs->lower->close(fs->lower, fh_lower);
        fs->upper->close(fs->upper, fh_upper);
        return VFS_ENOMEM;
    }

    while (1)
    {
        int read_sz = fs->lower->read(fs->lower, fh_lower, buf, buf_sz);
        if (read_sz == VFS_EOF)
        {
            break;
        }
        if (read_sz < 0)
        {
            ret = read_sz;
            break;
        }

        int write_sz = fs->upper->write(fs->upper, fh_upper, buf, read_sz);
        if (write_sz < 0)
        {
            ret = write_sz;
            break;
        }
        if (write_sz != read_sz)
        {
            ret = VFS_EIO;
            break;
        }
    }

    free(buf);
    fs->lower->close(fs->lower, fh_lower);
    fs->upper->close(fs->upper, fh_upper);
    return ret;
}

static int _vfs_overlayfs_open(struct vfs_operations* thiz, uintptr_t* fh,
    const char* path, uint64_t flags)
{
    int ret;
    vfs_stat_t info;
    vfs_overlayfs_t* fs = EV_CONTAINER_OF(thiz, vfs_overlayfs_t, op);
    vfs_str_t path_str = vfs_str_from_static1(path);
    vfs_str_t whiteout_path = VFS_STR_INIT;

    /* If file not exist and #VFS_O_CREATE not set, open should failed. */
    if ((ret = _vfs_overlayfs_common_stat_ex(fs, &path_str, &info, &whiteout_path)) < 0 && !(flags & VFS_O_CREATE))
    {
        vfs_str_exit(&whiteout_path);
        return VFS_ENOENT;
    }
    /* Either file exist or we need to create it. */

    /* If the file exist in upper or not exist at all, open it in upper directly. */
    if (ret == VFS_OVERLAYFS_STAT_UPPER || ret == VFS_OVERLAYFS_STAT_NOENT)
    {
        goto open_in_upper_layer;
    }

    /* If the file exist in lower layer, open it by flags. */
    if (ret == VFS_OVERLAYFS_STAT_LOWER)
    {
        if (flags & VFS_O_WRONLY)
        {
            if ((ret = _vfs_overlayfs_open_copy_to_upper(fs, &path_str)) != 0)
            {
                return ret;
            }
            goto open_in_upper_layer;
        }
        
        goto open_in_lower_layer;
    }

    /* If the file exist in lower layer, but whiteout. */
    if (ret == VFS_OVERLAYFS_STAT_WHITEOUT)
    {
        ret = _vfs_overlayfs_common_remove_whiteout_entry(fs, &whiteout_path);
        vfs_str_exit(&whiteout_path);
        if (ret != 0)
        {
            return ret;
        }

        if ((ret = vfs_path_ensure_parent_exist(fs->upper, &path_str)) != 0)
        {
            return ret;
        }

        goto open_in_upper_layer;
    }
    abort();

open_in_lower_layer:
    return _vfs_overlayfs_open_with_fs(fs, fs->lower, fh, &path_str, flags);
open_in_upper_layer:
    return _vfs_overlayfs_open_with_fs(fs, fs->upper, fh, &path_str, flags);
}

//////////////////////////////////////////////////////////////////////////
// close
//////////////////////////////////////////////////////////////////////////

static int _vfs_overlayfs_close(struct vfs_operations* thiz, uintptr_t fh)
{
    vfs_overlayfs_t* fs = EV_CONTAINER_OF(thiz, vfs_overlayfs_t, op);

    vfs_overlayfs_session_t* session = _vfs_overlayfs_common_find_session(fs, fh);
    if (session == NULL)
    {
        return VFS_EBADF;
    }

    /* Remove record. */
    vfs_map_erase(&fs->session_map, &session->node);

    /* Release session caused by #_vfs_overlayfs_common_find_session(). */
    _vfs_overlayfs_common_release_session(session);
    /* Release session caused by creation. */
    _vfs_overlayfs_common_release_session(session);

    return 0;
}

//////////////////////////////////////////////////////////////////////////
// truncate
//////////////////////////////////////////////////////////////////////////

typedef struct vfs_overlayfs_truncate_helper
{
    uint64_t    size;
} vfs_overlayfs_truncate_helper_t;

static int _vfs_overlayfs_truncate_inner(vfs_overlayfs_session_t* session, void* data)
{
    vfs_overlayfs_truncate_helper_t* helper = data;
    vfs_operations_t* fs = session->fs;

    if (fs->truncate == NULL)
    {
        return VFS_ENOSYS;
    }
    return fs->truncate(fs, session->real, helper->size);
}

static int _vfs_overlayfs_truncate(struct vfs_operations* thiz, uintptr_t fh, uint64_t size)
{
    vfs_overlayfs_t* fs = EV_CONTAINER_OF(thiz, vfs_overlayfs_t, op);
    vfs_overlayfs_truncate_helper_t helper = { size };
    return _vfs_overlayfs_common_fh(fs, fh, _vfs_overlayfs_truncate_inner, &helper);
}

//////////////////////////////////////////////////////////////////////////
// seek
//////////////////////////////////////////////////////////////////////////

typedef struct vfs_overlayfs_seek_helper
{
    int64_t offset;
    int     whence;
    int64_t ret;
} vfs_overlayfs_seek_helper_t;

static int _vfs_overlayfs_seek_inner(vfs_overlayfs_session_t* session, void* data)
{
    vfs_overlayfs_seek_helper_t* helper = data;
    vfs_operations_t* fs = session->fs;

    if (fs->seek == NULL)
    {
        return VFS_ENOSYS;
    }

    helper->ret = fs->seek(fs, session->real, helper->offset, helper->whence);
    return 0;
}

static int64_t _vfs_overlayfs_seek(struct vfs_operations* thiz, uintptr_t fh, int64_t offset, int whence)
{
    int ret;
    vfs_overlayfs_t* fs = EV_CONTAINER_OF(thiz, vfs_overlayfs_t, op);

    vfs_overlayfs_seek_helper_t helper = { offset, whence, 0 };
    ret = _vfs_overlayfs_common_fh(fs, fh, _vfs_overlayfs_seek_inner, &helper);
    if (ret != 0)
    {
        return ret;
    }
    return helper.ret;
}

//////////////////////////////////////////////////////////////////////////
// read
//////////////////////////////////////////////////////////////////////////

typedef struct vfs_overlayfs_read_helper
{
    void*   buf;
    size_t  len;
} vfs_overlayfs_read_helper_t;

static int _vfs_overlayfs_read_inner(vfs_overlayfs_session_t* session, void* data)
{
    vfs_overlayfs_read_helper_t* helper = data;
    vfs_operations_t* fs = session->fs;

    if (fs->read == NULL)
    {
        return VFS_ENOSYS;
    }
    return fs->read(fs, session->real, helper->buf, helper->len);
}

static int _vfs_overlayfs_read(struct vfs_operations* thiz, uintptr_t fh, void* buf, size_t len)
{
    vfs_overlayfs_t* fs = EV_CONTAINER_OF(thiz, vfs_overlayfs_t, op);
    vfs_overlayfs_read_helper_t helper = { buf, len };
    return _vfs_overlayfs_common_fh(fs, fh, _vfs_overlayfs_read_inner, &helper);
}

//////////////////////////////////////////////////////////////////////////
// write
//////////////////////////////////////////////////////////////////////////

typedef struct vfs_overlayfs_write_helper
{
    const void* buf;
    size_t      len;
} vfs_overlayfs_write_helper_t;

static int _vfs_overlayfs_write_inner(vfs_overlayfs_session_t* session, void* data)
{
    vfs_overlayfs_write_helper_t* helper = data;
    vfs_operations_t* fs = session->fs;

    if (fs->write == NULL)
    {
        return VFS_ENOSYS;
    }
    return fs->write(fs, session->real, helper->buf, helper->len);
}

static int _vfs_overlayfs_write(struct vfs_operations* thiz, uintptr_t fh, const void* buf, size_t len)
{
    vfs_overlayfs_t* fs = EV_CONTAINER_OF(thiz, vfs_overlayfs_t, op);
    vfs_overlayfs_write_helper_t helper = { buf, len };
    return _vfs_overlayfs_common_fh(fs, fh, _vfs_overlayfs_write_inner, &helper);
}

//////////////////////////////////////////////////////////////////////////
// mkdir
//////////////////////////////////////////////////////////////////////////

static int _vfs_overlayfs_mkdir_inner(vfs_overlayfs_t* fs, const vfs_str_t* path)
{
    /* Check if parent directory exist. */
    int ret = _vfs_overlayfs_common_is_parent_exist(fs, path);
    if (ret != 0)
    {
        return ret;
    }

    /* Parent directory exist, remove whiteout file if exist. */
    if ((ret = _vfs_overlayfs_common_remove_whiteout(fs, path)) != 0)
    {
        return ret;
    }

    /* Create directory. */
    return fs->upper->mkdir(fs->upper, path->str);
}

static int _vfs_overlayfs_mkdir(struct vfs_operations* thiz, const char* path)
{
    int ret;
    vfs_overlayfs_t* fs = EV_CONTAINER_OF(thiz, vfs_overlayfs_t, op);

    /* Make sure entry not logically exist. */
    vfs_stat_t info;
    if ((ret = thiz->stat(thiz, path, &info)) == 0)
    {
        return VFS_EEXIST;
    }

    vfs_str_t path_str = vfs_str_from_static1(path);
    return _vfs_overlayfs_mkdir_inner(fs, &path_str);
}

//////////////////////////////////////////////////////////////////////////
// rmdir
//////////////////////////////////////////////////////////////////////////

typedef struct vfs_overlayfs_rmdir_helper
{
    vfs_overlayfs_t*    fs;
    const char*         path;
} vfs_overlayfs_rmdir_helper_t;

static int _vfs_overlayfs_rmdir_create_whiteout_dir(vfs_overlayfs_t* fs,
    const vfs_str_t* path)
{
    int ret;
    vfs_str_t path_str = vfs_str_dup(path);
    vfs_str_append(&path_str, OVERLAY_WHITEOUT_SUFFIX, OVERLAY_WHITEOUT_SUFFIX_SZ);
    {
        ret = vfs_path_ensure_dir_exist(fs->upper, &path_str);
    }
    vfs_str_exit(&path_str);

    return ret;
}

static int _vfs_overlayfs_rmdir_cnt(const char* name, const vfs_stat_t* stat, void* data)
{
    (void)name; (void)stat;

    size_t* item_cnt = data;
    *item_cnt += 1;

    return 1;
}

static int _vfs_overlayfs_rmdir_recursion_children(const char* name, const vfs_stat_t* stat, void* data)
{
    vfs_overlayfs_rmdir_helper_t* helper = data;
    vfs_overlayfs_t* fs = helper->fs;

    vfs_str_t full_path = vfs_str_from1(helper->path);
    vfs_str_append1(&full_path, "/");
    vfs_str_append1(&full_path, name);

    if (stat->st_mode & VFS_S_IFREG)
    {
        fs->upper->unlink(fs->upper, full_path.str);
    }
    else
    {
        vfs_overlayfs_rmdir_helper_t tmp_helper = { fs, full_path.str };
        fs->upper->ls(fs->upper, full_path.str, _vfs_overlayfs_rmdir_recursion_children, &tmp_helper);
    }

    vfs_str_exit(&full_path);
    return 0;
}

static int _vfs_overlayfs_rmdir_recursion(vfs_overlayfs_t* fs, const char* path)
{
    int ret;

    vfs_overlayfs_rmdir_helper_t helper = { fs, path };
    ret = fs->upper->ls(fs->upper, path, _vfs_overlayfs_rmdir_recursion_children, &helper);
    if (ret != 0)
    {
        return ret;
    }

    return fs->upper->rmdir(fs->upper, path);
}

static int _vfs_overlayfs_rmdir(struct vfs_operations* thiz, const char* path)
{
    int ret;
    vfs_overlayfs_t* fs = EV_CONTAINER_OF(thiz, vfs_overlayfs_t, op);
    vfs_str_t path_str = vfs_str_from_static1(path);

    if (fs->upper->rmdir == NULL || fs->upper->stat == NULL || fs->lower->stat == NULL)
    {
        return VFS_ENOSYS;
    }

    /* Check if \p path is empty. */
    size_t item_cnt = 0;
    if ((ret = thiz->ls(thiz, path, _vfs_overlayfs_rmdir_cnt, &item_cnt)) != 0)
    {/* List failed. */
        return ret;
    }
    if (item_cnt != 0)
    {/* Not empty. */
        return VFS_ENOTEMPTY;
    }
    /* Now we are ready to rmdir. */

    /* Check if \p path is in upper layer. */
    vfs_stat_t info;
    if ((ret = fs->upper->stat(fs->upper, path, &info)) == VFS_ENOENT)
    {/* \p path not exist in upper layer. */
        goto finish;
    }

    /* Remove the \p path in upper layer. */
    if ((ret = _vfs_overlayfs_rmdir_recursion(fs, path)) != 0)
    {
        return ret;
    }

finish:
    /* Check if \p path is in lower layer. */
    if ((ret = fs->lower->stat(fs->lower, path, &info)) == 0)
    {/* Create whiteout directory. */
        return _vfs_overlayfs_rmdir_create_whiteout_dir(fs, &path_str);
    }
    return 0;
}

//////////////////////////////////////////////////////////////////////////
// unlink
//////////////////////////////////////////////////////////////////////////

static int _vfs_overlayfs_unlink_create_whiteout_file(vfs_overlayfs_t* fs,
    const char* path)
{
    int ret;
    vfs_str_t path_str = vfs_str_from_static1(path);
    vfs_str_append(&path_str, OVERLAY_WHITEOUT_SUFFIX, OVERLAY_WHITEOUT_SUFFIX_SZ);
    do {
        uintptr_t fh = 0;
        ret = fs->upper->open(fs->upper, &fh, path_str.str, VFS_O_WRONLY | VFS_O_CREATE);
        if (ret != 0)
        {
            break;
        }
        fs->upper->close(fs->upper, fh);
    } while (0);
    vfs_str_exit(&path_str);

    return ret;
}

static int _vfs_overlayfs_unlink(struct vfs_operations* thiz, const char* path)
{
    int ret;
    vfs_overlayfs_t* fs = EV_CONTAINER_OF(thiz, vfs_overlayfs_t, op);
    if (fs->upper->unlink == NULL || fs->lower->stat == NULL)
    {
        return VFS_ENOSYS;
    }

    /* Check if \p path is exist. */
    vfs_stat_t info;
    if ((ret = thiz->stat(thiz, path, &info)) != 0)
    {
        return ret;
    }
    if (!(info.st_mode & VFS_S_IFREG))
    {
        return VFS_EISDIR;
    }

    /* Try to remove the file in upper layer. */
    ret = fs->upper->unlink(fs->upper, path);
    if (ret != 0 && ret != VFS_ENOENT)
    {/* unlink failed with unknown error. */
        return ret;
    }
    /* The file now is not exist in upper layer. */

    /* Check the file in lower layer. */
    ret = fs->lower->stat(fs->lower, path, &info);
    if (ret == VFS_ENOENT)
    {/* No such file in lower layer. */
        return 0;
    }
    if (ret != 0)
    {/* unknown error. */
        return ret;
    }
    if (info.st_mode & VFS_S_IFDIR)
    {/* Path is a directory. */
        return VFS_EISDIR;
    }

    /* Create whiteout file in upper layer. */
    return _vfs_overlayfs_unlink_create_whiteout_file(fs, path);
}

//////////////////////////////////////////////////////////////////////////
// API
//////////////////////////////////////////////////////////////////////////

int vfs_make_overlay(vfs_operations_t** fs, vfs_operations_t* lower,
    vfs_operations_t* upper)
{
    vfs_overlayfs_t* overlayfs = calloc(1, sizeof(vfs_overlayfs_t));
    if (overlayfs == NULL)
    {
        return VFS_ENOMEM;
    }

    overlayfs->lower = lower;
    overlayfs->upper = upper;

    overlayfs->op.destroy = _vfs_overlayfs_destroy;
    overlayfs->op.ls = _vfs_overlayfs_ls;
    overlayfs->op.stat = _vfs_overlayfs_stat;
    overlayfs->op.open = _vfs_overlayfs_open;
    overlayfs->op.close = _vfs_overlayfs_close;
    overlayfs->op.truncate = _vfs_overlayfs_truncate;
    overlayfs->op.seek = _vfs_overlayfs_seek;
    overlayfs->op.read = _vfs_overlayfs_read;
    overlayfs->op.write = _vfs_overlayfs_write;
    overlayfs->op.mkdir = _vfs_overlayfs_mkdir;
    overlayfs->op.rmdir = _vfs_overlayfs_rmdir;
    overlayfs->op.unlink = _vfs_overlayfs_unlink;

    vfs_map_init(&overlayfs->session_map, _vfs_overlayfs_cmp_session, NULL);
    vfs_mutex_init(&overlayfs->session_map_lock);

    *fs = &overlayfs->op;
    return 0;
}
