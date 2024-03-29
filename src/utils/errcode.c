#include <stdlib.h>
#include "errcode.h"
#include "vfs/vfs.h"

/**
 * @brief Map native error code to vfs error code.
 */
#define VFS_ERR_MAP(xx) \
    xx(ENOENT)      \
    xx(EIO)         \
    xx(ENOMEM)      \
    xx(EACCES)      \
    xx(EEXIST)      \
    xx(ENOTDIR)     \
    xx(EISDIR)      \
    xx(EINVAL)      \
    xx(ENOSYS)      \
    xx(ENOTEMPTY)

static int _vfs_map_native_error_to_vfs(int errcode)
{
#define EXPAND_ERR_MAP_AS_TABLE(xx) case xx: return VFS_##xx;

    if (errcode < 0)
    {
        return errcode;
    }

    switch (errcode)
    {
        VFS_ERR_MAP(EXPAND_ERR_MAP_AS_TABLE);
    default:
        break;
    }

    abort();
#undef EXPAND_ERR_MAP_AS_TABLE
}

#if defined(_WIN32)

#include <windows.h>

int vfs_translate_sys_err(int errcode)
{
    switch (errcode)
    {
    case ERROR_FILE_EXISTS:         return -EALREADY;
    case ERROR_ALREADY_EXISTS:      return -EALREADY;
    case ERROR_FILE_NOT_FOUND:      return VFS_ENOENT;
    case ERROR_SHARING_VIOLATION:   return VFS_EACCES;

    default:
        break;
    }

    return _vfs_map_native_error_to_vfs(errcode);
}

#else

int vfs_translate_sys_err(int errcode)
{
    return _vfs_map_native_error_to_vfs(errcode);
}

#endif
