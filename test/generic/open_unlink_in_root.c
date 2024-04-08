#include <string.h>
#include <stdlib.h>
#include "__init__.h"

static void _vfs_test_generic_open_unlink_in_root(vfs_operations_t* fs)
{
    const char* path = "/generic_fs_tester";

    vfs_stat_t info;
    const size_t path_sz = strlen(path);

    {
        uintptr_t fh;
        ASSERT_EQ_INT(fs->open(fs, &fh, path, VFS_O_WRONLY | VFS_O_CREATE), 0);
        ASSERT_EQ_INT(fs->write(fs, fh, path, path_sz), (int)path_sz);
        ASSERT_EQ_INT(fs->close(fs, fh), 0);

        ASSERT_EQ_INT(fs->stat(fs, path, &info), 0);
        ASSERT_EQ_UINT64(info.st_mode, VFS_S_IFREG);
        ASSERT_EQ_UINT64(info.st_size, (int)path_sz);
    }

    {
        uintptr_t fh;
        size_t buf_sz = path_sz * 2;
        char* buf = malloc(path_sz + 1);
        if (buf == NULL)
        {
            abort();
        }

        ASSERT_EQ_INT(fs->open(fs, &fh, path, VFS_O_RDONLY), 0);
        ASSERT_EQ_INT(fs->read(fs, fh, buf, buf_sz), (int)path_sz);
        ASSERT_EQ_INT(fs->close(fs, fh), 0);
        ASSERT_EQ_INT(memcmp(buf, path, path_sz), 0);
        free(buf);
    }

    ASSERT_EQ_INT(fs->unlink(fs, path), 0);
    ASSERT_EQ_INT(fs->stat(fs, path, &info), VFS_ENOENT);
}

const vfs_test_generic_case_t vfs_test_generic_open_unlink_in_root = {
    "open_unlink_in_root", _vfs_test_generic_open_unlink_in_root,
};
