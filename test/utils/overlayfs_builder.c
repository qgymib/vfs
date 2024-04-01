#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include "test.h"
#include "vfs/fs/localfs.h"
#include "vfs/fs/overlayfs.h"
#include "utils/cwdpath.h"
#include "utils/path.h"
#include "overlayfs_builder.h"

typedef struct overlayfs_rmdir_helper
{
    vfs_operations_t*   fs;
    const vfs_str_t*    path;
} overlayfs_rmdir_helper_t;

static void _overlayfs_quick_ensure_dir(overlayfs_quick_t* quick, const vfs_str_t* path)
{
    vfs_operations_t* fs = quick->fs_local;
    ASSERT_EQ_INT(vfs_path_ensure_dir_exist(fs, path), 0);
}

static void _overlayfs_quick_build_generic_item(overlayfs_quick_t* quick, const vfs_str_t* path, vfs_stat_flag_t type)
{
    if (type & VFS_S_IFDIR)
    {
        _overlayfs_quick_ensure_dir(quick, path);
        return;
    }

    vfs_str_t parent = vfs_path_parent(path, NULL);
    if (!VFS_STR_IS_EMPTY(&parent))
    {
        _overlayfs_quick_ensure_dir(quick, &parent);
    }
    vfs_str_exit(&parent);

    {
        uintptr_t fh = 0;
        vfs_operations_t* fs = quick->fs_local;
        ASSERT_EQ_INT(fs->open(fs, &fh, path->str, VFS_O_WRONLY | VFS_O_CREATE), 0);
        ASSERT_EQ_INT(fs->write(fs, fh, "dummy", 5), 5);
        ASSERT_EQ_INT(fs->close(fs, fh), 0);
    }
}

static void _overlayfs_quick_build_generic(overlayfs_quick_t* quick, const vfs_fsbuilder_item_t* lower, const vfs_str_t* prefix)
{
    size_t idx = 0;
    for (; lower[idx].name != NULL; idx++)
    {
        const vfs_fsbuilder_item_t* item = &lower[idx];

        vfs_str_t path = vfs_str_dup(prefix);
        vfs_str_append1(&path, item->name);
        {
            _overlayfs_quick_build_generic_item(quick, &path, item->type);
        }
        vfs_str_exit(&path);
    }
}

static void _overlayfs_quick_build_lower(overlayfs_quick_t* quick, const vfs_fsbuilder_item_t* lower)
{
    _overlayfs_quick_build_generic(quick, lower, &quick->lower_path);
}

static void _overlayfs_quick_build_upper(overlayfs_quick_t* quick, const vfs_fsbuilder_item_t* upper)
{
    _overlayfs_quick_build_generic(quick, upper, &quick->upper_path);
}

static int _overlayfs_quick_rmdir_lscb(const char* name, const vfs_stat_t* stat, void* data)
{
    overlayfs_rmdir_helper_t* helper = data;
    vfs_operations_t* fs = helper->fs;

    vfs_str_t full_path = vfs_str_dup(helper->path);
    vfs_str_append1(&full_path, "/");
    vfs_str_append1(&full_path, name);
    if (stat->st_mode & VFS_S_IFREG)
    {
        ASSERT_EQ_INT(fs->unlink(fs, full_path.str), 0);
    }
    else
    {
        overlayfs_rmdir_helper_t tmp = { fs, &full_path };
        ASSERT_EQ_INT(fs->ls(fs, full_path.str, _overlayfs_quick_rmdir_lscb, &tmp), 0);
        ASSERT_EQ_INT(fs->rmdir(fs, full_path.str), 0);
    }
    vfs_str_exit(&full_path);

    return 0;
}

static void _overlayfs_quick_rmdir(overlayfs_quick_t* quick, const vfs_str_t* path)
{
    int ret;
    vfs_operations_t* fs = quick->fs_local;

    overlayfs_rmdir_helper_t helper = { fs, path };
    ret = fs->ls(fs, path->str, _overlayfs_quick_rmdir_lscb, &helper);
    ASSERT_EQ_INT(ret == 0 || ret == VFS_ENOENT, 1, "ret:%d", ret);

    ret = fs->rmdir(fs, path->str);
    ASSERT_EQ_INT(ret == 0 || ret == VFS_ENOENT, 1, "ret:%d", ret);
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
    ASSERT_EQ_INT(vfs_make_local(&quick->fs_lower, quick->lower_path.str), 0);

    quick->upper_path = vfs_str_dup(&quick->cwd_path);
    vfs_str_append1(&quick->upper_path, "/upper");
    _overlayfs_quick_ensure_dir(quick, &quick->upper_path);
    ASSERT_EQ_INT(vfs_make_local(&quick->fs_upper, quick->upper_path.str), 0);

    ASSERT_EQ_INT(vfs_make_overlay(&quick->fs_overlay, quick->fs_lower, quick->fs_upper), 0);
    ASSERT_EQ_INT(vfs_mount(mount, quick->fs_overlay), 0);

    if (lower != NULL)
    {
        _overlayfs_quick_build_lower(quick, lower);
    }
    if (upper != NULL)
    {
        _overlayfs_quick_build_upper(quick, upper);
    }

    return quick;
}

void vfs_overlayfs_quick_exit(overlayfs_quick_t* quick)
{
    vfs_unmount(quick->mount.str);
    _overlayfs_quick_rmdir(quick, &quick->cwd_path);

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
