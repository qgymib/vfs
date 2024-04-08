#include "test.h"
#include "vfs/fs/localfs.h"
#include "generic/__init__.h"

static vfs_operations_t* s_test_localfs_generic = NULL;

TEST_FIXTURE_SETUP(localfs)
{
    ASSERT_EQ_INT(0, vfs_init());

    ASSERT_EQ_INT(vfs_make_local(&s_test_localfs_generic, g_cwd_path.str), 0);
    ASSERT_NE_PTR(s_test_localfs_generic, NULL);
}

TEST_FIXTURE_TEARDOWN(localfs)
{
    s_test_localfs_generic->destroy(s_test_localfs_generic);
    s_test_localfs_generic = NULL;

    vfs_exit();
}

TEST_F(localfs, generic)
{
    vfs_test_generic(s_test_localfs_generic);
}
