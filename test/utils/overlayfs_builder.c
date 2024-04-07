#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include "test.h"
#include "vfs/fs/localfs.h"
#include "vfs/fs/overlayfs.h"
#include "vfs/utils/file.h"
#include "utils/cwdpath.h"
#include "utils/dir.h"
#include "overlayfs_builder.h"

typedef struct overlayfs_rmdir_helper
{
    vfs_operations_t*   fs;
    const vfs_str_t*    path;
} overlayfs_rmdir_helper_t;

static void _overlayfs_quick_ensure_dir(overlayfs_quick_t* quick, const vfs_str_t* path)
{
    vfs_operations_t* fs = quick->fs_local;
    ASSERT_EQ_INT(vfs_dir_make(fs, path->str), 0,
        "path:%s", path->str);
}

static vfs_str_t _overlayfs_getcwd(void)
{
    vfs_str_t unix_cwd = VFS_STR_INIT;
#if defined(_WIN32)
    vfs_str_append1(&unix_cwd, "/");
#endif
    {
        vfs_str_t cwd = vfs_test_cwd_path();
        vfs_path_to_unix(&cwd);
        vfs_str_append2(&unix_cwd, &cwd);
        vfs_str_exit(&cwd);
    }

    {
        time_t rawtime;
        time(&rawtime);

        struct tm* timeinfo = localtime(&rawtime);

        char buffer[128];
        strftime(buffer, sizeof(buffer), "/%Y%m%dT%H%M%S", timeinfo);

        vfs_str_append1(&unix_cwd, buffer);
    }

    {
        char buffer[128];
        int randval = rand();
        snprintf(buffer, sizeof(buffer), "-%d", randval);
        vfs_str_append1(&unix_cwd, buffer);
    }

    return unix_cwd;
}

overlayfs_quick_t* vfs_overlayfs_quick_make(const char* mount,
    const vfs_fsbuilder_item_t* lower, const vfs_fsbuilder_item_t* upper)
{
    overlayfs_quick_t* quick = calloc(1, sizeof(overlayfs_quick_t));
    ASSERT_NE_PTR(quick, NULL);
    quick->mount = vfs_str_from1(mount);

    quick->cwd_path = _overlayfs_getcwd();
    ASSERT_EQ_INT(vfs_make_local(&quick->fs_local, "/"), 0);

    quick->lower_path = vfs_str_dup(&quick->cwd_path);
    vfs_str_append1(&quick->lower_path, "/lower");
    _overlayfs_quick_ensure_dir(quick, &quick->lower_path);
    ASSERT_EQ_INT(vfs_make_local(&quick->fs_lower, quick->lower_path.str), 0,
        "path:%s", quick->lower_path.str);

    quick->upper_path = vfs_str_dup(&quick->cwd_path);
    vfs_str_append1(&quick->upper_path, "/upper");
    _overlayfs_quick_ensure_dir(quick, &quick->upper_path);
    ASSERT_EQ_INT(vfs_make_local(&quick->fs_upper, quick->upper_path.str), 0);

    ASSERT_EQ_INT(vfs_make_overlay(&quick->fs_overlay, quick->fs_lower, quick->fs_upper), 0);
    ASSERT_EQ_INT(vfs_mount(mount, quick->fs_overlay), 0);

    if (lower != NULL)
    {
        vfs_test_fs_build(quick->fs_local, lower, quick->lower_path.str);
    }
    if (upper != NULL)
    {
        vfs_test_fs_build(quick->fs_local, upper, quick->upper_path.str);
    }

    return quick;
}

void vfs_overlayfs_quick_exit(overlayfs_quick_t* quick)
{
    vfs_unmount(quick->mount.str);
    vfs_dir_delete(quick->fs_local, quick->cwd_path.str);

    if (quick->fs_local != NULL)
    {
        quick->fs_local->destroy(quick->fs_local);
        quick->fs_local = NULL;
    }

    vfs_str_exit(&quick->lower_path);
    vfs_str_exit(&quick->upper_path);
    vfs_str_exit(&quick->mount);
    vfs_str_exit(&quick->cwd_path);
    free(quick);
}
