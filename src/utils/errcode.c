#include <stdlib.h>
#include "errcode.h"
#include "vfs/vfs.h"

/**
 * @brief Map native error code to vfs error code.
 */
#define VFS_ERR_MAP(xx) \
    xx(ENOENT)      \
    xx(EIO)         \
    xx(EBADF)       \
    xx(ENOMEM)      \
    xx(EACCES)      \
    xx(EEXIST)      \
    xx(ENOTDIR)     \
    xx(EISDIR)      \
    xx(EINVAL)      \
    xx(ESPIPE)      \
    xx(ENOSYS)      \
    xx(ENOTEMPTY)   \
    xx(EALREADY)

int vfs_translate_posix_error(int errcode)
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
    case ERROR_FILE_NOT_FOUND:      return VFS_ENOENT;
    case ERROR_PATH_NOT_FOUND:      return VFS_ENOENT;
    case ERROR_ACCESS_DENIED:       return VFS_EACCES;
    case ERROR_SHARING_VIOLATION:   return VFS_EACCES;
    case ERROR_FILE_EXISTS:         return VFS_EALREADY;
    case ERROR_DIR_NOT_EMPTY:       return VFS_ENOTEMPTY;
    case ERROR_ALREADY_EXISTS:      return VFS_EALREADY;
    case ERROR_DIRECTORY:           return VFS_ENOTDIR;
    default:
        break;
    }

    return vfs_translate_posix_error(errcode);
}

#else

int vfs_translate_sys_err(int errcode)
{
    return vfs_translate_posix_error(errcode);
}

#endif
