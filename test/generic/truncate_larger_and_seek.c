#include <string.h>
#include <stdlib.h>
#include "__init__.h"

static void _vfs_test_generic_truncate_larger_and_seek(vfs_operations_t* fs)
{
    const char* path = "/truncate_larger";

    uintptr_t fh;
    const size_t path_sz = strlen(path);
    const size_t path_twice_sz = path_sz * 2;

    /* Create file and write data. */
    ASSERT_EQ_INT(fs->open(fs, &fh, path, VFS_O_RDWR | VFS_O_CREATE), 0);
    ASSERT_EQ_INT(fs->write(fs, fh, path, path_sz), (int)path_sz);

    /* Trunk file. */
    ASSERT_EQ_INT(fs->truncate(fs, fh, path_twice_sz), 0);
    /* Use seek to check file size. */
    ASSERT_EQ_INT64(fs->seek(fs, fh, 0, VFS_SEEK_END), path_twice_sz);

    /* Check file content. */
    {
        size_t buf_sz = path_twice_sz + 1;
        char* buf = malloc(buf_sz);
        if (buf == NULL)
        {
            abort();
        }

        ASSERT_EQ_INT64(fs->seek(fs, fh, 0, VFS_SEEK_SET), 0);
        ASSERT_EQ_INT(fs->read(fs, fh, buf, path_sz), (int)path_sz);
        ASSERT_EQ_INT(memcmp(buf, path, path_sz), 0);

        ASSERT_EQ_INT(fs->read(fs, fh, buf, path_sz), (int)path_sz);
        size_t i;
        for (i = 0; i < path_sz; i++)
        {
            ASSERT_EQ_CHAR(buf[i], '\0');
        }

        free(buf);
    }

    /* Cleanup. */
    ASSERT_EQ_INT(fs->close(fs, fh), 0);
    ASSERT_EQ_INT(fs->unlink(fs, path), 0);
}

const vfs_test_generic_case_t vfs_test_generic_truncate_larget_and_seek = {
    "truncate_larget_and_seek", _vfs_test_generic_truncate_larger_and_seek,
};
