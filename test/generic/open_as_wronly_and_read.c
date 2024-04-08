#include <string.h>
#include "utils/file.h"
#include "__init__.h"

static void _vfs_test_generic_open_as_wronly_and_read(vfs_operations_t* fs)
{
    const char* path = "/fb18187d-fe52-4519-a1e8-ade46a01762a";
    const size_t path_sz = strlen(path);
    ASSERT_EQ_INT(vfs_file_write(fs, path, VFS_O_WRONLY | VFS_O_CREATE, path, path_sz), (int)path_sz);

    uintptr_t fh;
    ASSERT_EQ_INT(fs->open(fs, &fh, path, VFS_O_WRONLY), 0);

    char buf[8];

    /*
     * According to `man 2 read`, `EBADF` should be returned if fh is not open for reading.
     */
    ASSERT_EQ_INT(fs->read(fs, fh, buf, sizeof(buf)), VFS_EBADF);

    ASSERT_EQ_INT(fs->unlink(fs, path), 0);
}

const vfs_test_generic_case_t vfs_test_generic_open_as_wronly_and_read = {
    "open_as_wronly_and_read", _vfs_test_generic_open_as_wronly_and_read,
};
