#include "test.h"
#include "vfs/fs/memfs.h"
#include "utils/generic_fs_tester.h"

static vfs_operations_t* s_test_memfs_generic = NULL;

TEST_FIXTURE_SETUP(memfs)
{
    ASSERT_EQ_INT(0, vfs_init());

    ASSERT_EQ_INT(vfs_make_memory(&s_test_memfs_generic), 0);
    ASSERT_NE_PTR(s_test_memfs_generic, NULL);
}

TEST_FIXTURE_TEARDOWN(memfs)
{
    s_test_memfs_generic->destroy(s_test_memfs_generic);
    s_test_memfs_generic = NULL;

    vfs_exit();
}

TEST_F(memfs, generic)
{
    vfs_test_generic(s_test_memfs_generic);
}
