#if defined(_WIN32)
#   include <direct.h>
#else
#   include <unistd.h>
#endif

#include <stdlib.h>
#include "cwdpath.h"

vfs_str_t vfs_test_cwd_path(void)
{
    char buff[4096];
    char* cwd = getcwd(buff, sizeof(buff));
    if (cwd == NULL)
    {
        abort();
    }

    vfs_str_t ret = VFS_STR_INIT;
    vfs_str_append1(&ret, cwd);
    return ret;
}
