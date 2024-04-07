#include <assert.h>
#include "dir.h"

typedef struct vfs_dir_delete_helper
{
    vfs_operations_t*   fs;         /**< The file system we are working on. */
    int                 errcode;    /**< Error code. */
    vfs_str_t           path;       /**< The path need to delete. */
} vfs_dir_delete_helper_t;

static void _vfs_path_to_generic(vfs_str_t* str, char s, char t)
{
    size_t i;

    vfs_str_ensure_dynamic(str);

    for (i = 0; i < str->len; i++)
    {
        if (str->str[i] == s)
        {
            str->str[i] = t;
        }
    }
}

static int _vfs_dir_delete_on_ls(const char* name, const vfs_stat_t* stat, void* data)
{
    vfs_dir_delete_helper_t* helper = data;

    vfs_str_t full_path = vfs_str_dup(&helper->path);
    vfs_str_append1(&full_path, "/");
    vfs_str_append1(&full_path, name);
    do
    {
        if (stat->st_mode & VFS_S_IFREG)
        {
            helper->errcode = helper->fs->unlink(helper->fs, full_path.str);
            break;
        }
        helper->errcode = vfs_dir_delete(helper->fs, full_path.str);
    } while (0);
    vfs_str_exit(&full_path);
    return helper->errcode != 0 ? 1 : 0;
}

/**
 * @brief Search for the exist parent directory of \p path.
 * @param[in] fs - The file system.
 * @param[in] path - The path.
 * @return - The exist parent directory of \p path, or empty string if not found.
 */
static int _vfs_dir_search_for_exist_parent(vfs_operations_t* fs, vfs_str_t* dir, const vfs_str_t* path)
{
    int ret;
    vfs_str_reset(dir);

    if (fs->stat == NULL)
    {
        return VFS_ENOSYS;
    }

    vfs_stat_t info;
    vfs_str_t parent = vfs_str_dup(path);

    while ((ret = fs->stat(fs, parent.str, &info)) != 0)
    {
        vfs_str_t tmp = vfs_path_parent(&parent, NULL);
        vfs_str_exit(&parent);
        parent = tmp;

        if (VFS_STR_IS_EMPTY(&parent))
        {
            ret = VFS_ENOENT;
            break;
        }
    }

    if (ret == 0)
    {
        vfs_str_append2(dir, &parent);
    }

    vfs_str_exit(&parent);
    return ret;
}

/**
 * @brief Create directories from \p root to \p path.
 * @note \p root must be an exist directory.
 * @note \p root must be a substring of \p path.
 * @param[in,out] fs - The file system.
 * @param[in] root - The root directory.
 * @param[in] path - The path to be created.
 * @return - 0: on success.
 * @return - -errno: on error.
 */
static int _vfs_dir_mkdir(vfs_operations_t* fs, const vfs_str_t* root, const vfs_str_t* path)
{
    int ret;
    if (root->len == path->len)
    {
        return 0;
    }
    assert(root->len < path->len);

    size_t pos;
    /*
     * We are starting from `root->len+2`, that's because:
     * 1. `root->len` refer to the root, which is always an exist directory.
     * 2. `root->len+1` refer to the root plus '/'.
     */
    for (pos = root->len + 2; pos < path->len; pos++)
    {
        if (path->str[pos] == '/')
        {
            vfs_str_t tmp = vfs_str_sub(path, 0, pos);
            ret = fs->mkdir(fs, tmp.str);
            vfs_str_exit(&tmp);

            if (ret)
            {
                return ret;
            }
        }
    }

    /* Finally mkdir the element. */
    return fs->mkdir(fs, path->str);
}

vfs_str_t vfs_path_parent(const vfs_str_t* path, vfs_str_t* basename)
{
    vfs_str_t parent = VFS_STR_INIT;
    if (vfs_str_cmp1(path, "/") == 0)
    {
        if (basename != NULL)
        {
            vfs_str_reset(basename);
        }
        return parent;
    }

    ptrdiff_t pos = path->len - 1;
    for (; pos >= 0; pos--)
    {
        if (path->str[pos] == '/')
        {
            break;
        }
    }

    if (pos == 0)
    {
        if (basename != NULL)
        {
            vfs_str_reset(basename);
            vfs_str_append(basename, path->str + 1, path->len - 1);
        }
        return vfs_str_from_static1("/");
    }

    if (basename != NULL)
    {
        vfs_str_reset(basename);
        vfs_str_append(basename, path->str + pos + 1, path->len - pos - 1);
    }
    return vfs_str_sub(path, 0, pos);
}

void vfs_path_to_native(vfs_str_t* str)
{
#if defined(_WIN32)
    vfs_path_to_win32(str);
#else
    vfs_path_to_unix(str);
#endif
}

void vfs_path_to_unix(vfs_str_t* str)
{
    _vfs_path_to_generic(str, '\\', '/');
}

void vfs_path_to_win32(vfs_str_t* str)
{
    _vfs_path_to_generic(str, '/', '\\');
}

vfs_str_t vfs_path_layer(const vfs_str_t* path, size_t level)
{
    assert(path->str[0] == '/');

    if (level == 0)
    {
        return vfs_str_from_static1("/");
    }

    size_t cnt = 0;
    for (size_t i = 0; i < path->len; i++)
    {
        if (path->str[i] == '/')
        {
            if (cnt == level)
            {
                return vfs_str_from(path->str, i);
            }
            cnt++;
        }
    }

    if (cnt == level)
    {
        return vfs_str_dup(path);
    }

    vfs_str_t empty = VFS_STR_INIT;
    return empty;
}

int vfs_path_is_native_root(const vfs_str_t* path)
{
#if defined(_WIN32)
    const char* root = "\\";
#else
    const char* root = "/";
#endif

    return vfs_str_cmp1(path, root) == 0;
}

int vfs_path_is_root(const vfs_str_t* path)
{
    return vfs_str_cmp1(path, "/") == 0 || vfs_str_cmp1(path, "\\") == 0;
}

int vfs_path_ensure_dir_exist(vfs_operations_t* fs, const vfs_str_t* path)
{
    int ret;
    vfs_str_t root = VFS_STR_INIT; 

    if ((ret = _vfs_dir_search_for_exist_parent(fs, &root, path)) == 0)
    {
        ret = _vfs_dir_mkdir(fs, &root, path);
    }
    vfs_str_exit(&root);

    return ret;
}

int vfs_path_ensure_parent_exist(vfs_operations_t* fs, const vfs_str_t* path)
{
    vfs_str_t parent = vfs_path_parent(path, NULL);
    if (VFS_STR_IS_EMPTY(&parent))
    {
        return VFS_EIO;
    }

    int ret = vfs_path_ensure_dir_exist(fs, &parent);
    vfs_str_exit(&parent);

    return ret;
}

int vfs_dir_make(vfs_operations_t* fs, const char* path)
{
    int ret = 0;
    vfs_str_t path_str = vfs_str_from_static1(path);

    vfs_str_t parent = vfs_path_parent(&path_str, NULL);
    do 
    {
        if ((ret = vfs_path_ensure_dir_exist(fs, &parent)) != 0)
        {
            break;
        }
        if (fs->mkdir == NULL)
        {
            ret = VFS_ENOSYS;
            break;
        }
        ret = fs->mkdir(fs, path);
    } while (0);
    vfs_str_exit(&parent);

    return ret;
}

int vfs_dir_delete(vfs_operations_t* fs, const char* path)
{
    int ret;
    vfs_dir_delete_helper_t helper = { fs, 0, VFS_STR_INIT };
    helper.path = vfs_str_from_static1(path);

    if (fs->ls == NULL || fs->rmdir == NULL || fs->unlink == NULL)
    {
        return VFS_ENOSYS;
    }

    if ((ret = fs->ls(fs, path, _vfs_dir_delete_on_ls, &helper)) != 0)
    {
        return ret;
    }
    if (helper.errcode != 0)
    {
        return helper.errcode;
    }

    return fs->rmdir(fs, path);
}
