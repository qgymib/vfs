#include <string.h>
#include "test.h"
#include "vfs/fs/localfs.h"
#include "utils/fsbuilder.h"

#define LOCALFS_TEST_MOUNT_PATH "file:///foo/bar/"
#define LOCALFS_TEST_MAGIC      "122c41eb-739d-4da7-b605-634e57a28d3d"

TEST_FIXTURE_SETUP(localfs)
{
    ASSERT_EQ_INT(0, vfs_init());

    /* Mount to #LOCALFS_TEST_MOUNT_PATH */
    {
        vfs_operations_t* fs = NULL;
        ASSERT_EQ_INT(vfs_make_local(&fs, "/"), 0);
        ASSERT_NE_PTR(fs, NULL);
        ASSERT_EQ_INT(vfs_mount(LOCALFS_TEST_MOUNT_PATH, fs), 0);
    }
}

TEST_FIXTURE_TEARDOWN(localfs)
{
    vfs_exit();
}

TEST_F(localfs, seek)
{
    vfs_operations_t* vfs = vfs_visitor_instance();
    ASSERT_NE_PTR(vfs, NULL);

    vfs_str_t file_path = vfs_str_from1(LOCALFS_TEST_MOUNT_PATH);
#if !defined(_WIN32)
    vfs_str_chop(&file_path, 1);
#endif
    vfs_str_append2(&file_path, &g_exe_path);

    vfs_str_t exe_data = VFS_STR_INIT;
    vfs_test_read_file(vfs, file_path.str, &exe_data);

    size_t offset = 0;
    {
        const char* pattern = LOCALFS_TEST_MAGIC;
        const size_t pattern_sz = strlen(pattern);
        offset = vfs_str_search2(&exe_data, pattern, pattern_sz);
        ASSERT_GE_SIZE(offset, 0);
    }

    char buffer[sizeof(LOCALFS_TEST_MAGIC) - 1];
    {
        uintptr_t fh;
        ASSERT_EQ_INT(vfs->open(vfs, &fh, file_path.str, VFS_O_RDONLY), 0);
        ASSERT_EQ_INT64(vfs->seek(vfs, fh, offset, VFS_SEEK_SET), offset);

        ASSERT_EQ_INT(vfs->read(vfs, fh, buffer, sizeof(buffer)), sizeof(buffer));
        ASSERT_EQ_INT(memcmp(buffer, LOCALFS_TEST_MAGIC, sizeof(buffer)), 0);

        ASSERT_EQ_INT(vfs->close(vfs, fh), 0);
    }

    vfs_str_exit(&file_path);
    vfs_str_exit(&exe_data);
}
