#include "test.h"
#include "vfs/fs/randfs.h"

static vfs_operations_t* s_test_randfs = NULL;

TEST_FIXTURE_SETUP(randfs)
{
    ASSERT_EQ_INT(0, vfs_init());

    ASSERT_EQ_INT(vfs_make_random(&s_test_randfs), 0);
}

TEST_FIXTURE_TEARDOWN(randfs)
{
    if (s_test_randfs != NULL)
    {
        s_test_randfs->destroy(s_test_randfs);
        s_test_randfs = NULL;
    }

    vfs_exit();
}

//////////////////////////////////////////////////////////////////////////
// randfs.stat
//////////////////////////////////////////////////////////////////////////

TEST_F(randfs, stat)
{
    vfs_stat_t info;

    ASSERT_EQ_INT(s_test_randfs->stat(s_test_randfs, "/", &info), 0);
    ASSERT_EQ_UINT64(info.st_mode, VFS_S_IFDIR);

    ASSERT_EQ_INT(s_test_randfs->stat(s_test_randfs, "/random", &info), 0);
    ASSERT_EQ_UINT64(info.st_mode, VFS_S_IFREG);

    ASSERT_EQ_INT(s_test_randfs->stat(s_test_randfs, "/urandom", &info), VFS_ENOENT);
}

//////////////////////////////////////////////////////////////////////////
// randfs.ls
//////////////////////////////////////////////////////////////////////////

static int _test_vfs_randfs_ls(const char* name, const vfs_stat_t* stat, void* data)
{
    (void)name; (void)stat;
    size_t* p_cnt = data;
    *p_cnt += 1;
    return 0;
}

TEST_F(randfs, ls)
{
    size_t cnt = 0;
    ASSERT_EQ_INT(s_test_randfs->ls(s_test_randfs, "/", _test_vfs_randfs_ls, &cnt), 0);
    ASSERT_EQ_SIZE(cnt, 1);
}

//////////////////////////////////////////////////////////////////////////
// randfs.read
//////////////////////////////////////////////////////////////////////////

TEST_F(randfs, read)
{
    int num;

    uintptr_t fh;
    ASSERT_EQ_INT(s_test_randfs->open(s_test_randfs, &fh, "/random", VFS_O_RDONLY), 0);
    ASSERT_EQ_INT(s_test_randfs->read(s_test_randfs, fh, &num, sizeof(num)), sizeof(num));
    ASSERT_EQ_INT(s_test_randfs->close(s_test_randfs, fh), 0);
}

//////////////////////////////////////////////////////////////////////////
// randfs.write
//////////////////////////////////////////////////////////////////////////

TEST_F(randfs, write)
{
    int num = 0;

    uintptr_t fh;
    ASSERT_EQ_INT(s_test_randfs->open(s_test_randfs, &fh, "/random", VFS_O_RDONLY), 0);
    ASSERT_EQ_INT(s_test_randfs->write(s_test_randfs, fh, &num, sizeof(num)), sizeof(num));
    ASSERT_EQ_INT(s_test_randfs->close(s_test_randfs, fh), 0);
}

//////////////////////////////////////////////////////////////////////////
// randfs.truncate
//////////////////////////////////////////////////////////////////////////

TEST_F(randfs, truncate)
{
    uintptr_t fh;
    ASSERT_EQ_INT(s_test_randfs->open(s_test_randfs, &fh, "/random", VFS_O_RDONLY), 0);
    ASSERT_EQ_INT(s_test_randfs->truncate(s_test_randfs, fh, 0), VFS_EINVAL);
    ASSERT_EQ_INT(s_test_randfs->close(s_test_randfs, fh), 0);
}

//////////////////////////////////////////////////////////////////////////
// randfs.seek
//////////////////////////////////////////////////////////////////////////

TEST_F(randfs, seek)
{
    uintptr_t fh;
    ASSERT_EQ_INT(s_test_randfs->open(s_test_randfs, &fh, "/random", VFS_O_RDONLY), 0);
    ASSERT_EQ_INT64(s_test_randfs->seek(s_test_randfs, fh, 0, VFS_SEEK_SET), VFS_ESPIPE);
    ASSERT_EQ_INT(s_test_randfs->close(s_test_randfs, fh), 0);
}
