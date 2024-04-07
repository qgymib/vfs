#include "utils/str.h"
#include "utils/dir.h"
#include "file.h"

static int _vfs_file_open_directly(vfs_operations_t* fs, uintptr_t* fh, const char* path, uint64_t flags)
{
    if (fs->open == NULL)
    {
        return VFS_ENOSYS;
    }
    return fs->open(fs, fh, path, flags);
}

static int _vfs_file_open_creat(vfs_operations_t* fs, uintptr_t* fh, const char* path, uint64_t flags)
{
    int ret = 0;
    vfs_str_t path_str = vfs_str_from_static1(path);

    vfs_str_t basename = VFS_STR_INIT;
    vfs_str_t parent = vfs_path_parent(&path_str, &basename);
    do
    {
        if (VFS_STR_IS_EMPTY(&parent))
        {
            ret = VFS_EINVAL;
            break;
        }
        if (flags & VFS_O_CREATE)
        {
            if ((ret = vfs_path_ensure_dir_exist(fs, &parent)) != 0)
            {
                break;
            }
        }
        ret = _vfs_file_open_directly(fs, fh, path, flags);
    } while (0);
    vfs_str_exit(&parent);
    vfs_str_exit(&basename);

    return ret;
}

int vfs_file_open(vfs_operations_t* fs, uintptr_t* fh, const char* path, uint64_t flags)
{
    if (!(flags & VFS_O_CREATE))
    {
        return _vfs_file_open_directly(fs, fh, path, flags);
    }

    return _vfs_file_open_creat(fs, fh, path, flags);
}

int vfs_file_write(vfs_operations_t* fs, const char* path, uint64_t flags,
    const void* buf, size_t len)
{
    int ret = 0;
    uintptr_t fh = 0;
    if (fs->write == NULL || fs->close == NULL)
    {
        return VFS_ENOSYS;
    }

    if ((ret = vfs_file_open(fs, &fh, path, flags)) != 0)
    {
        return ret;
    }

    ret = fs->write(fs, fh, buf, len);
    fs->close(fs, fh);

    return ret;
}
