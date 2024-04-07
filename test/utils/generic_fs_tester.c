#include <string.h>
#include <stdlib.h>
#include "utils/dir.h"
#include "utils/file.h"
#include "test.h"
#include "generic_fs_tester.h"

static int _vfs_test_generic_count_cb(const char* name, const vfs_stat_t* stat, void* data)
{
    (void)name; (void)stat;

    size_t* cnt = data;
    *cnt += 1;

    return 0;
}

static void _vfs_test_generic_stat_root(vfs_operations_t* fs)
{
    vfs_stat_t info;
    ASSERT_EQ_INT(fs->stat(fs, "/", &info), 0);
    ASSERT_EQ_UINT64(info.st_mode, VFS_S_IFDIR);
}

static size_t _vfs_test_generic_count_item(vfs_operations_t* fs, const char* path)
{
    size_t cnt = 0;
    ASSERT_EQ_INT(fs->ls(fs, path, _vfs_test_generic_count_cb, &cnt), 0);
    return cnt;
}

static void _vfs_test_generic_mkdir_rmdir(vfs_operations_t* fs, const char* path)
{
    vfs_stat_t info;

    /* mkdir */
    ASSERT_EQ_INT(fs->mkdir(fs, path), 0);
    ASSERT_EQ_INT(fs->stat(fs, path, &info), 0);
    ASSERT_EQ_UINT64(info.st_mode, VFS_S_IFDIR);

    /* rmdir */
    ASSERT_EQ_INT(fs->rmdir(fs, path), 0);
    ASSERT_EQ_INT(fs->stat(fs, path, &info), VFS_ENOENT);
}

static void _vfs_test_generic_create_unlink(vfs_operations_t* fs, const char* path)
{
    vfs_stat_t info;
    const size_t path_sz = strlen(path);

    {
        uintptr_t fh;
        ASSERT_EQ_INT(fs->open(fs, &fh, path, VFS_O_WRONLY | VFS_O_CREATE), 0);
        ASSERT_EQ_INT(fs->write(fs, fh, path, path_sz), (int)path_sz);
        ASSERT_EQ_INT(fs->close(fs, fh), 0);

        ASSERT_EQ_INT(fs->stat(fs, path, &info), 0);
        ASSERT_EQ_UINT64(info.st_mode, VFS_S_IFREG);
        ASSERT_EQ_UINT64(info.st_size, (int)path_sz);
    }

    {
        uintptr_t fh;
        size_t buf_sz = path_sz * 2;
        char* buf = malloc(path_sz + 1);
        ASSERT_NE_PTR(buf, NULL);
        ASSERT_EQ_INT(fs->open(fs, &fh, path, VFS_O_RDONLY), 0);
        ASSERT_EQ_INT(fs->read(fs, fh, buf, buf_sz), (int)path_sz);
        ASSERT_EQ_INT(fs->close(fs, fh), 0);
        ASSERT_EQ_INT(memcmp(buf, path, path_sz), 0);
        free(buf);
    }
    
    ASSERT_EQ_INT(fs->unlink(fs, path), 0);
    ASSERT_EQ_INT(fs->stat(fs, path, &info), VFS_ENOENT);
}

static void _vfs_test_generic_truncate_smaller_and_seek(vfs_operations_t* fs, const char* path)
{
    uintptr_t fh;
    const size_t path_sz = strlen(path);
    const size_t path_half_sz = path_sz / 2;

    /* Create file and write data. */
    ASSERT_EQ_INT(fs->open(fs, &fh, path, VFS_O_RDWR | VFS_O_CREATE), 0);
    ASSERT_EQ_INT(fs->write(fs, fh, path, path_sz), (int)path_sz);

    /* Trunk file. */
    ASSERT_EQ_INT(fs->truncate(fs, fh, path_half_sz), 0);
    /* Use seek to check file size. */
    ASSERT_EQ_INT64(fs->seek(fs, fh, 0, VFS_SEEK_END), path_half_sz);

    /* Check file content. */
    {
        size_t buf_sz = path_sz + 1;
        char* buf = malloc(buf_sz);
        ASSERT_NE_PTR(buf, NULL);
        ASSERT_EQ_INT64(fs->seek(fs, fh, 0, VFS_SEEK_SET), 0);
        ASSERT_EQ_INT(fs->read(fs, fh, buf, buf_sz), (int)path_half_sz);
        ASSERT_EQ_INT(memcmp(buf, path, path_half_sz), 0);
        free(buf);
    }
    
    /* Cleanup. */
    ASSERT_EQ_INT(fs->close(fs, fh), 0);
    ASSERT_EQ_INT(fs->unlink(fs, path), 0);
}

static void _vfs_test_generic_truncate_larger_and_seek(vfs_operations_t* fs, const char* path)
{
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
        ASSERT_NE_PTR(buf, NULL);

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

static void _vfs_test_generic_rmdir_mismatch(vfs_operations_t* fs)
{
    uintptr_t fh;
    const char* path = "/rmdir_mismatch";
    ASSERT_EQ_INT(fs->open(fs, &fh, path, VFS_O_WRONLY | VFS_O_CREATE), 0);
    ASSERT_EQ_INT(fs->close(fs, fh), 0);

    vfs_stat_t info;
    ASSERT_EQ_INT(fs->stat(fs, path, &info), 0);
    ASSERT_EQ_UINT64(info.st_mode, VFS_S_IFREG);

    /* try to rmdir a file. */
    ASSERT_EQ_INT(fs->rmdir(fs, path), VFS_ENOTDIR);

    ASSERT_EQ_INT(fs->unlink(fs, path), 0);
    ASSERT_EQ_INT(fs->stat(fs, path, &info), VFS_ENOENT);
}

static void _vfs_test_generic_unlink_mismatch(vfs_operations_t* fs)
{
    const char* path = "/unlink_mismatch";

    ASSERT_EQ_INT(fs->mkdir(fs, path), 0);

    vfs_stat_t info;
    ASSERT_EQ_INT(fs->stat(fs, path, &info), 0);
    ASSERT_EQ_UINT64(info.st_mode, VFS_S_IFDIR);

    /* try to unlink a directory. */
    ASSERT_EQ_INT(fs->unlink(fs, path), VFS_EISDIR);

    ASSERT_EQ_INT(fs->rmdir(fs, path), 0);
    ASSERT_EQ_INT(fs->stat(fs, path, &info), VFS_ENOENT);
}

void vfs_test_generic(vfs_operations_t* fs)
{
    /* Check the root stat of file system. */
    _vfs_test_generic_stat_root(fs);
    size_t root_sz = _vfs_test_generic_count_item(fs, "/");

    /* mkdir and rmdir. */
    {
        const char* path = "/generic_fs_tester";
        _vfs_test_generic_mkdir_rmdir(fs, path);
    }

    /* file creation and unlink. */
    {
        const char* path = "/generic_fs_tester";
        _vfs_test_generic_create_unlink(fs, path);
    }

    _vfs_test_generic_truncate_smaller_and_seek(fs, "/truncate_smaller");
    _vfs_test_generic_truncate_larger_and_seek(fs, "/truncate_larger");

    _vfs_test_generic_rmdir_mismatch(fs);
    _vfs_test_generic_unlink_mismatch(fs);

    /* create directory whose parent is not exist. */
    ASSERT_EQ_INT(fs->mkdir(fs, "/foo/bar"), VFS_ENOENT);

    /* create file whose parent is not exist. */
    {
        uintptr_t fh;
        ASSERT_EQ_INT(fs->open(fs, &fh, "/foo/bar", VFS_O_WRONLY | VFS_O_CREATE), VFS_ENOENT);
    }

    /* rmdir non-empty directory. */
    {
        ASSERT_EQ_INT(vfs_dir_make(fs, "/foo/bar"), 0);
        ASSERT_EQ_INT(fs->rmdir(fs, "/foo"), VFS_ENOTEMPTY);
        ASSERT_EQ_INT(vfs_dir_delete(fs, "/foo"), 0);
    }
    {
        uintptr_t fh;
        ASSERT_EQ_INT(vfs_file_open(fs, &fh, "/hello/world", VFS_O_WRONLY | VFS_O_CREATE), 0);
        ASSERT_EQ_INT(fs->close(fs, fh), 0);
        ASSERT_EQ_INT(fs->rmdir(fs, "/hello"), VFS_ENOTEMPTY);
        ASSERT_EQ_INT(vfs_dir_delete(fs, "/hello"), 0);
    }

    /* After all test, the root state should remain the same. */
    ASSERT_EQ_SIZE(_vfs_test_generic_count_item(fs, "/"), root_sz);
}
