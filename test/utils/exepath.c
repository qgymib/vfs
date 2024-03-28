#include "exepath.h"

#if defined(_WIN32)

#include <windows.h>
#include <assert.h>

vfs_str_t vfs_test_exe_path(void)
{
    vfs_str_t ret = VFS_STR_INIT;
    vfs_str_resize(&ret, 32768);

    ret.len = GetModuleFileNameA(NULL, ret.str, (DWORD)ret.cap);
    assert(ret.len < ret.cap);
    ret.str[ret.len] = '\0';

    return ret;
}

#else

#include <unistd.h>
#include <stdlib.h>

vfs_str_t vfs_test_exe_path(void)
{
    vfs_str_t ret = VFS_STR_INIT;
    vfs_str_resize(&ret, 32768);

    ssize_t read_sz = readlink("/proc/self/exe", ret.str, ret.cap);
    if (read_sz < 0)
    {
        abort();
    }
    ret.len = read_sz;

    return ret;
}

#endif
