#include "test.h"
#include "utils/cwdpath.h"
#include "utils/exepath.h"

vfs_str_t g_exe_path = VFS_STR_INIT;
vfs_str_t g_cwd_path = VFS_STR_INIT;

void vfs_test_init(int argc, char* argv[])
{
    (void)argc; (void)argv;

    g_exe_path = vfs_test_exe_path();
    g_cwd_path = vfs_test_cwd_path();
}

void vfs_test_exit(void)
{
    vfs_str_exit(&g_exe_path);
    vfs_str_exit(&g_cwd_path);
}
