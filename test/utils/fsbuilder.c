#include "test.h"
#include "vfs/vfs.h"
#include "fsbuilder.h"

static void _vfs_test_fs_build_item(vfs_operations_t* fs, const vfs_str_t* path, vfs_stat_flag_t type)
{
    if (type & VFS_S_IFDIR)
    {
        ASSERT_EQ_INT(vfs_dir_make(fs, path->str), 0);
        return;
    }

    ASSERT_EQ_INT(vfs_file_write(fs, path->str, VFS_O_WRONLY | VFS_O_CREATE, "dummy", 5), 5);
}

void vfs_test_fs_build(vfs_operations_t* fs, const vfs_fsbuilder_item_t* items,
    const char* prefix)
{
    size_t i;
    for (i = 0; items[i].name != NULL; i++)
    {
        const vfs_fsbuilder_item_t* item = &items[i];

        vfs_str_t path = VFS_STR_INIT;
        if (prefix != NULL)
        {
            vfs_str_append1(&path, prefix);
        }
        vfs_str_append1(&path, item->name);

        _vfs_test_fs_build_item(fs, &path, item->type);
        vfs_str_exit(&path);
    }
}

void vfs_test_read_file(vfs_operations_t* fs, const char* path, vfs_str_t* dat)
{
    uintptr_t fh;
    ASSERT_EQ_INT(fs->open(fs, &fh, path, VFS_O_RDONLY), 0);

    char buf[4096];
    int read_sz;

    while ((read_sz = fs->read(fs, fh, buf, sizeof(buf))) > 0)
    {
        vfs_str_append(dat, buf, read_sz);
    }
    ASSERT_EQ_INT(read_sz, VFS_EOF);

    ASSERT_EQ_INT(fs->close(fs, fh), 0);
}
