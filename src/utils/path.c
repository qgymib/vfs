#include <assert.h>
#include "path.h"

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
    return vfs_str_sub(path, 0, pos - 1);
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
