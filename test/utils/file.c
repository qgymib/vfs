#include "test.h"
#include "vfs/vfs.h"
#include "file.h"

void vfs_test_read_file(vfs_str_t* dat, const char* path)
{
    vfs_operations_t* vfs = vfs_visitor();
    ASSERT_NE_PTR(vfs, NULL);

    uintptr_t fh;
    ASSERT_EQ_INT(vfs->open(vfs, &fh, path, VFS_O_RDONLY), 0);

    char buf[4096];
    int read_sz;

    while ((read_sz = vfs->read(vfs, fh, buf, sizeof(buf))) > 0)
    {
        vfs_str_append(dat, buf, read_sz);
    }
    ASSERT_EQ_INT(read_sz, VFS_EOF);

    ASSERT_EQ_INT(vfs->close(vfs, fh), 0);
}
