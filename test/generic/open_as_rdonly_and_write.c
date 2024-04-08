#include <string.h>
#include "utils/file.h"
#include "__init__.h"

static void _vfs_test_generic_open_as_rdonly_and_write(vfs_operations_t* fs)
{
    const char* path = "/bdd2547c-5ce9-4b41-bdec-0945582a868c";
    const size_t path_sz = strlen(path);
    ASSERT_EQ_INT(vfs_file_write(fs, path, VFS_O_WRONLY | VFS_O_CREATE, path, path_sz), (int)path_sz);

    uintptr_t fh;
    ASSERT_EQ_INT(fs->open(fs, &fh, path, VFS_O_RDONLY), 0);

    /*
     * According to `man 2 write`, `EBADF` should be returned if fh is not open for writing.
     */
    ASSERT_EQ_INT(fs->write(fs, fh, path, path_sz), VFS_EBADF);

    ASSERT_EQ_INT(fs->unlink(fs, path), 0);
}

const vfs_test_generic_case_t vfs_test_generic_open_as_rdonly_and_write = {
    "open_as_rdonly_and_write", _vfs_test_generic_open_as_rdonly_and_write,
};
