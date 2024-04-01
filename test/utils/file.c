#include "test.h"
#include "vfs/vfs.h"
#include "file.h"

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

void vfs_test_write_file(vfs_operations_t* fs, const char* path, const void* data, size_t size)
{
    uintptr_t fh = 0;
    ASSERT_EQ_INT(fs->open(fs, &fh, path, VFS_O_WRONLY | VFS_O_CREATE), 0);
    ASSERT_EQ_SIZE(fs->write(fs, fh, data, size), size);
    ASSERT_EQ_INT(fs->close(fs, fh), 0);
}
