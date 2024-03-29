#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include "vfs/fs/localfs.h"
#include "utils/defs.h"
#include "utils/path.h"
#include "utils/errcode.h"

typedef struct vfs_localfs
{
    vfs_operations_t    op;
    vfs_str_t           root;
} vfs_localfs_t;

#if defined(_WIN32)

#include <windows.h>
#include <assert.h>
#include <direct.h>
#include <sys/types.h>
#include <sys/stat.h>

#define rmdir(p)    _rmdir(p)
#define unlink(f)   _unlink(f)
#define stat(p, b)  _stat(p, b)

typedef struct _stat vfs_nativate_stat_t;

static uint64_t _vfs_win_filetime_2_utc(const FILETIME* t)
{
    ULARGE_INTEGER ui;
    ui.LowPart = t->dwLowDateTime;
    ui.HighPart = t->dwHighDateTime;

    const uint64_t ret = ui.QuadPart / 10000000ULL - 11644473600ULL;
    return ret;
}

static vfs_stat_t _vfs_win_find_data_to_vfs_stat(const WIN32_FIND_DATAA* src)
{
    vfs_stat_t info = { 0,0,0 };

    if (src->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
    {
        info.st_mode |= VFS_S_IFDIR;
    }
    else
    {
        info.st_mode |= VFS_S_IFREG;
    }

    LARGE_INTEGER file_size;
    file_size.LowPart = src->nFileSizeLow;
    file_size.HighPart = src->nFileSizeHigh;
    info.st_size = file_size.QuadPart;

    info.st_mtime = _vfs_win_filetime_2_utc(&src->ftLastWriteTime);

    return info;
}

static int _vfs_local_ls_drivers(vfs_ls_cb fn, void* data)
{
    vfs_str_t logical_drives = VFS_STR_INIT;
    vfs_str_t drive_letter = VFS_STR_INIT;
    vfs_str_resize(&logical_drives, MAX_PATH);

    DWORD dwResult = GetLogicalDriveStringsA((DWORD)logical_drives.cap - 1, logical_drives.str);
    if (dwResult > logical_drives.cap - 1)
    {
        vfs_str_resize(&logical_drives, dwResult + 1);
        dwResult = GetLogicalDriveStringsA((DWORD)logical_drives.cap - 1, logical_drives.str);
        assert(dwResult < logical_drives.cap);
    }
    assert(dwResult > 0);

    const vfs_stat_t info = {
        VFS_S_IFDIR, 0, 0,
    };
    char* szSingleDrive = logical_drives.str;
    assert(szSingleDrive != NULL);

    while (*szSingleDrive)
    {
        vfs_str_reset(&drive_letter);
        vfs_str_append1(&drive_letter, szSingleDrive);
        vfs_str_remove_trailing(&drive_letter, '\\');

        if (fn(drive_letter.str, &info, data) != 0)
        {
            break;
        }
        szSingleDrive += strlen(szSingleDrive) + 1;
    }

    vfs_str_exit(&logical_drives);
    vfs_str_exit(&drive_letter);

    return 0;
}

static int _vfs_local_ls_common(const vfs_str_t* path, vfs_ls_cb fn, void* data)
{
    if (strcmp(path->str, "/") == 0)
    {
        return _vfs_local_ls_drivers(fn, data);
    }

    int ret = 0;
    WIN32_FIND_DATAA ffd;
    HANDLE hFind = INVALID_HANDLE_VALUE;
    vfs_str_t copy_path = vfs_str_dup(path);
    vfs_str_append1(&copy_path, "/*");
    vfs_path_to_native(&copy_path);

    do
    {
        if ((hFind = FindFirstFileA(copy_path.str, &ffd)) == INVALID_HANDLE_VALUE)
        {
            ret = -ENOENT;
            break;
        }

        do 
        {
            if (strcmp(ffd.cFileName, ".") == 0 || strcmp(ffd.cFileName, "..") == 0)
            {
                continue;
            }

            const vfs_stat_t info = _vfs_win_find_data_to_vfs_stat(&ffd);
            if (fn(ffd.cFileName, &info, data) != 0)
            {
                break;
            }

        } while (FindNextFileA(hFind, &ffd) != 0);
    } while (0);
    
    if (hFind != INVALID_HANDLE_VALUE)
    {
        FindClose(hFind);
    }
    vfs_str_exit(&copy_path);

    return ret;
}

static DWORD _vfs_win_flags_to_desired_access(uint64_t flags)
{
    int ret = 0;
    if (flags & VFS_O_WRONLY)
    {
        ret |= GENERIC_WRITE;
    }
    if (flags & VFS_O_RDONLY)
    {
        ret |= GENERIC_READ;
    }
    if (flags & VFS_O_APPEND)
    {
        ret |= FILE_APPEND_DATA;
    }
    return ret;
}

static DWORD _vfs_win_flags_to_creation_disposition(uint64_t flags)
{
    int ret = 0;
    if (flags & VFS_O_CREATE)
    {
        ret |= OPEN_ALWAYS;
    }
    else
    {
        ret |= OPEN_EXISTING;
    }
    if (flags & VFS_O_TRUNCATE)
    {
        ret |= TRUNCATE_EXISTING;
    }
    return ret;
}

static DWORD _vfs_win_flags_to_share_mode(uint64_t flags)
{
    int ret = FILE_SHARE_DELETE;
    if (flags & VFS_O_RDONLY)
    {
        ret |= FILE_SHARE_READ;
    }
    if (flags & VFS_O_WRONLY)
    {
        ret |= FILE_SHARE_WRITE;
    }
    return ret;
}

static int _vfs_localfs_open_common(uintptr_t* fh, const vfs_str_t* path, uint64_t flags)
{
    int ret = 0;
    DWORD dwDesiredAccess = _vfs_win_flags_to_desired_access(flags);
    DWORD dwCreationDisposition = _vfs_win_flags_to_creation_disposition(flags);
    DWORD dwShareMode = _vfs_win_flags_to_share_mode(flags);
    HANDLE fileHandle = CreateFileA(path->str, dwDesiredAccess, dwShareMode,
        NULL, dwCreationDisposition, FILE_ATTRIBUTE_NORMAL, NULL);
    if (fileHandle == INVALID_HANDLE_VALUE)
    {
        DWORD errcode = GetLastError();
        ret = vfs_translate_sys_err(errcode);
        goto finish;
    }

    *fh = (uintptr_t)fileHandle;

finish:
    return ret;
}

static int _vfs_localfs_close(struct vfs_operations* thiz, uintptr_t fh)
{
    (void)thiz;
    HANDLE file_handle = (HANDLE)fh;
    CloseHandle(file_handle);
    return 0;
}

static int _vfs_localfs_read(struct vfs_operations* thiz, uintptr_t fh, void* buf, size_t len)
{
    (void)thiz;
    HANDLE file_handle = (HANDLE)fh;

    DWORD read_sz = 0;
    if (!ReadFile(file_handle, buf, (DWORD)len, &read_sz, NULL))
    {
        DWORD errcode = GetLastError();
        return vfs_translate_sys_err(errcode);
    }

    if (read_sz == 0)
    {
        return VFS_EOF;
    }

    return read_sz;
}

static int _vfs_localfs_write(struct vfs_operations* thiz, uintptr_t fh, const void* buf, size_t len)
{
    (void)thiz;
    HANDLE file_handle = (HANDLE)fh;

    DWORD write_sz = 0;
    if (!WriteFile(file_handle, buf, (DWORD)len, &write_sz, NULL))
    {
        DWORD errcode = GetLastError();
        return vfs_translate_sys_err(errcode);
    }

    return write_sz;
}

static vfs_stat_t _vfs_localfs_stat_to_vfs(const struct _stat* src)
{
    vfs_stat_t info = {
        0,
        src->st_size,
        src->st_mtime,
    };

    if ((src->st_mode & S_IFMT) == S_IFREG)
    {
        info.st_mode |= VFS_S_IFREG;
    }
    if ((src->st_mode & S_IFMT) == S_IFDIR)
    {
        info.st_mode |= VFS_S_IFDIR;
    }

    return info;
}

static DWORD _vfs_localfs_vfs_whence_to_win32(int whence)
{
    switch (whence)
    {
    case VFS_SEEK_SET:  return FILE_BEGIN;
    case VFS_SEEK_CUR:  return FILE_CURRENT;
    case VFS_SEEK_END:  return FILE_END;
    default:            break;
    }
    abort();
}

static int64_t _vfs_localfs_seek(struct vfs_operations* thiz, uintptr_t fh, int64_t offset, int whence)
{
    (void)thiz;
    HANDLE file_handle = (HANDLE)fh;

    LARGE_INTEGER win_offset;
    win_offset.QuadPart = offset;
    LONG lDistanceToMove = win_offset.LowPart;
    LONG* lpDistanceToMoveHigh = &win_offset.HighPart;
    DWORD dwMoveMethod = _vfs_localfs_vfs_whence_to_win32(whence);

    win_offset.LowPart = SetFilePointer(file_handle, lDistanceToMove, lpDistanceToMoveHigh, dwMoveMethod);
    if (win_offset.LowPart == INVALID_SET_FILE_POINTER)
    {
        DWORD errcode = GetLastError();
        return vfs_translate_sys_err(errcode);
    }

    return win_offset.QuadPart;
}

static int _vfs_localfs_mkdir_common(const vfs_str_t* path)
{
    if (_mkdir(path->str) < 0)
    {
        int errcode = errno;
        return vfs_translate_sys_err(errcode);
    }
    return 0;
}

#else

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>

typedef struct stat vfs_nativate_stat_t;

static vfs_stat_t _vfs_localfs_stat_to_vfs(const struct stat* src)
{
    vfs_stat_t info = {
        0,
        src->st_size,
        src->st_mtim.tv_sec,
    };

    if ((src->st_mode & S_IFMT) == S_IFREG)
    {
        info.st_mode |= VFS_S_IFREG;
    }
    if ((src->st_mode & S_IFMT) == S_IFDIR)
    {
        info.st_mode |= VFS_S_IFDIR;
    }

    return info;
}

static int _vfs_local_ls_common(const vfs_str_t* path, vfs_ls_cb fn, void* data)
{
    int ret = 0;
    DIR* dp = NULL;
    vfs_str_t copy_path = vfs_str_dup(path);

    do 
    {
        if ((dp = opendir(path->str)) == NULL)
        {
            ret = vfs_translate_sys_err(errno);
            break;
        }

        struct dirent* d;
        while ((d = readdir(dp)) != NULL)
        {
            vfs_str_resize(&copy_path, path->len);

            vfs_str_append1(&copy_path, "/");
            vfs_str_append1(&copy_path, d->d_name);

            struct stat statbuf;
            if (stat(copy_path.str, &statbuf) < 0)
            {
                ret = vfs_translate_sys_err(errno);
                break;
            }
            const vfs_stat_t info = _vfs_localfs_stat_to_vfs(&statbuf);

            if ((ret = fn(d->d_name, &info, data)) != 0)
            {
                ret = 0;
                break;
            }
        }
    } while (0);

    vfs_str_exit(&copy_path);
    if (dp != NULL)
    {
        closedir(dp);
    }

    return ret;
}

static int _vfs_localfs_flags_to_linux_flags(uint64_t flags)
{
    int ret = 0;
    if ((flags & VFS_O_RDWR) == VFS_O_RDWR)
    {
        ret |= O_RDWR;
    }
    else if (flags & VFS_O_WRONLY)
    {
        ret |= O_WRONLY;
    }
    else if (flags & VFS_O_RDONLY)
    {
        ret |= O_RDONLY;
    }

    if (flags & VFS_O_APPEND)
    {
        ret |= O_APPEND;
    }
    if (flags & VFS_O_TRUNCATE)
    {
        ret |= O_TRUNC;
    }
    if (flags & VFS_O_CREATE)
    {
        ret |= O_CREAT;
    }

    return ret;
}

static mode_t _vfs_localfs_flags_to_linux_mode(uint64_t flags)
{
    if (!(flags & VFS_O_CREATE))
    {
        return 0;
    }

    return S_IRWXU;
}

static int _vfs_localfs_open_common(uintptr_t* fh, const vfs_str_t* path, uint64_t flags)
{
    const int native_flag = _vfs_localfs_flags_to_linux_flags(flags);
    const mode_t native_mode = _vfs_localfs_flags_to_linux_mode(flags);
    int fd = open(path->str, native_flag, native_mode);
    if (fd < 0)
    {
        return vfs_translate_sys_err(errno);
    }

    *fh = fd;
    return 0;
}

static int _vfs_localfs_close(struct vfs_operations* thiz, uintptr_t fh)
{
    (void)thiz;
    int fd = fh;
    close(fd);
    return 0;
}

static int _vfs_localfs_read(struct vfs_operations* thiz, uintptr_t fh, void* buf, size_t len)
{
    (void)thiz;
    int fd = fh;

    ssize_t read_sz = read(fd, buf, len);
    if (read_sz < 0)
    {
        return vfs_translate_sys_err(errno);
    }
    else if (read_sz == 0)
    {
        return VFS_EOF;
    }

    return read_sz;
}

static int _vfs_localfs_write(struct vfs_operations* thiz, uintptr_t fh, const void* buf, size_t len)
{
    (void)thiz;
    int fd = fh;

    ssize_t write_sz = write(fd, buf, len);
    if (write_sz < 0)
    {
        return vfs_translate_sys_err(errno);
    }
    return write_sz;
}

static int _vfs_localfs_vfs_whence_to_linux(int whence)
{
    switch (whence)
    {
    case VFS_SEEK_SET:  return SEEK_SET;
    case VFS_SEEK_CUR:  return SEEK_CUR;
    case VFS_SEEK_END:  return SEEK_END;
    default:            break;
    }
    abort();
}

static int64_t _vfs_localfs_seek(struct vfs_operations* thiz, uintptr_t fh, int64_t offset, int whence)
{
    (void)thiz;

    int fd = fh;
    whence = _vfs_localfs_vfs_whence_to_linux(whence);

    off_t ret = lseek(fd, offset, whence);
    if (ret < 0)
    {
        int errcode = errno;
        return vfs_translate_sys_err(errcode);
    }
    return ret;
}

static int _vfs_localfs_mkdir_common(const vfs_str_t* path)
{
    if (mkdir(path->str, 0) < 0)
    {
        int errcode = errno;
        return vfs_translate_sys_err(errcode);
    }
    return 0;
}

#endif

static void _vfs_local_destroy(struct vfs_operations* thiz)
{
    vfs_localfs_t* fs = EV_CONTAINER_OF(thiz, vfs_localfs_t, op);
    vfs_str_exit(&fs->root);
    free(fs);
}

static vfs_str_t _vfs_local_get_access_path(const vfs_str_t* root, const char* path)
{
    vfs_str_t local_path = vfs_str_dup(root);

    /* If \p root is '/', ignore it because \p path always contains leading slash. */
    if (strcmp(local_path.str, "/") == 0)
    {
        vfs_str_reset(&local_path);
    }
    vfs_str_append1(&local_path, path);

    /*
     * For windows the leading '/' should be removed.
     */
#if defined(_WIN32)
    if (local_path.len >= 2 && local_path.str[0] == '/' && local_path.str[1] != '/')
    {
        vfs_str_t new_path = vfs_str_sub1(&local_path, 1);
        vfs_str_exit(&local_path);
        local_path = new_path;
    }
#endif

    return local_path;
}

static int _vfs_local_ls(struct vfs_operations* thiz, const char* path, vfs_ls_cb fn, void* data)
{
    int ret;
    vfs_localfs_t* fs = EV_CONTAINER_OF(thiz, vfs_localfs_t, op);

    vfs_str_t tmp_path = _vfs_local_get_access_path(&fs->root, path);
    {
        ret = _vfs_local_ls_common(&tmp_path, fn, data);
    }
    vfs_str_exit(&tmp_path);

    return ret;
}

static int _vfs_localfs_open(struct vfs_operations* thiz, uintptr_t* fh, const char* path, uint64_t flags)
{
    int ret;
    vfs_localfs_t* fs = EV_CONTAINER_OF(thiz, vfs_localfs_t, op);

    vfs_str_t tmp_path = _vfs_local_get_access_path(&fs->root, path);
    {
        ret = _vfs_localfs_open_common(fh, &tmp_path, flags);
    }
    vfs_str_exit(&tmp_path);

    return ret;
}

static int _vfs_localfs_stat_common(const vfs_str_t* path, vfs_stat_t* info)
{
    vfs_nativate_stat_t buf;

    if (stat(path->str, &buf) < 0)
    {
        int errcode = errno;
        return vfs_translate_sys_err(errcode);
    }

    *info = _vfs_localfs_stat_to_vfs(&buf);
    return 0;
}

static int _vfs_localfs_stat(struct vfs_operations* thiz, const char* path, vfs_stat_t* info)
{
    int ret;
    vfs_localfs_t* fs = EV_CONTAINER_OF(thiz, vfs_localfs_t, op);

    vfs_str_t actual_path = _vfs_local_get_access_path(&fs->root, path);
    {
        ret = _vfs_localfs_stat_common(&actual_path, info);
    }
    vfs_str_exit(&actual_path);

    return ret;
}

static int _vfs_localfs_mkdir(struct vfs_operations* thiz, const char* path)
{
    int ret;
    vfs_localfs_t* fs = EV_CONTAINER_OF(thiz, vfs_localfs_t, op);

    vfs_str_t actual_path = _vfs_local_get_access_path(&fs->root, path);
    {
        ret = _vfs_localfs_mkdir_common(&actual_path);
    }
    vfs_str_exit(&actual_path);

    return ret;
}

static int _vfs_localfs_rmdir_common(const vfs_str_t* path)
{
    if (rmdir(path->str) == 0)
    {
        return 0;
    }

    int ret = errno;
    ret = vfs_translate_sys_err(ret);

    return ret;
}

static int _vfs_localfs_rmdir(struct vfs_operations* thiz, const char* path)
{
    int ret;
    vfs_localfs_t* fs = EV_CONTAINER_OF(thiz, vfs_localfs_t, op);

    vfs_str_t actual_path = _vfs_local_get_access_path(&fs->root, path);
    {
        ret = _vfs_localfs_rmdir_common(&actual_path);
    }
    vfs_str_exit(&actual_path);

    return ret;
}

static int _vfs_localfs_unlink_common(const vfs_str_t* path)
{
    if (unlink(path->str) == 0)
    {
        return 0;
    }

    int ret = errno;
    ret = vfs_translate_sys_err(ret);

    return ret;
}

static int _vfs_localfs_unlink(struct vfs_operations* thiz, const char* path)
{
    int ret;
    vfs_localfs_t* fs = EV_CONTAINER_OF(thiz, vfs_localfs_t, op);

    vfs_str_t actual_path = _vfs_local_get_access_path(&fs->root, path);
    {
        ret = _vfs_localfs_unlink_common(&actual_path);
    }
    vfs_str_exit(&actual_path);

    return ret;
}

int vfs_make_local(vfs_operations_t** fs, const char* root)
{
    vfs_localfs_t* newfs = calloc(1, sizeof(vfs_localfs_t));
    if (newfs == NULL)
    {
        return -ENOMEM;
    }
    newfs->root = vfs_str_from1(root);

    newfs->op.destroy = _vfs_local_destroy;
    newfs->op.ls = _vfs_local_ls;
    newfs->op.stat = _vfs_localfs_stat;
    newfs->op.open = _vfs_localfs_open;
    newfs->op.close = _vfs_localfs_close;
    newfs->op.seek = _vfs_localfs_seek;
    newfs->op.read = _vfs_localfs_read;
    newfs->op.write = _vfs_localfs_write;
    newfs->op.mkdir = _vfs_localfs_mkdir;
    newfs->op.rmdir = _vfs_localfs_rmdir;
    newfs->op.unlink = _vfs_localfs_unlink;

    *fs = &newfs->op;
    return 0;
}
