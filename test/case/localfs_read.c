#include <string.h>
#include "test.h"
#include "vfs/fs/localfs.h"
#include "utils/file.h"

#define LOCALFS_TEST_MOUNT_PATH "/vfs"
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

TEST_F(localfs, read)
{
    vfs_operations_t* vfs = vfs_visitor_instance();
    ASSERT_NE_PTR(vfs, NULL);

    vfs_str_t mount_path = vfs_str_from1(LOCALFS_TEST_MOUNT_PATH);
#if defined(_WIN32)
    vfs_str_append1(&mount_path, "/");
#endif
    vfs_str_append2(&mount_path, &g_exe_path);

    vfs_str_t exe_data = VFS_STR_INIT;
    vfs_test_read_file(vfs, mount_path.str, &exe_data);

    /* Check the magic string. */
    {
        const char* pattern = LOCALFS_TEST_MAGIC;
        const size_t pattern_sz = strlen(pattern);
        ASSERT_GE_PTRDIFF(vfs_str_search2(&exe_data, pattern, pattern_sz), 0);
    }

    vfs_str_exit(&mount_path);
    vfs_str_exit(&exe_data);
}
