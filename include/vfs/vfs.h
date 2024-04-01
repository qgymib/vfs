#ifndef __VFS_H__
#define __VFS_H__

#include <stdint.h>
#include <stddef.h>
#include "inner/errno.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum vfs_open_flag
{
    VFS_O_RDONLY    = 0x0001,                       /**< Read only. */
    VFS_O_WRONLY    = 0x0002,                       /**< Write only. */
    VFS_O_RDWR      = VFS_O_RDONLY | VFS_O_WRONLY,  /**< Read and write. */

    VFS_O_APPEND    = 0x0004,                       /**< Append to file. The file must exist. It is conflict with #VFS_O_TRUNCATE. */
    VFS_O_TRUNCATE  = 0x0008,                       /**< Truncate file to zero. The file must exist. It is conflict with #VFS_O_APPEND. */
    VFS_O_CREATE    = 0x0010,                       /**< Create file if not exist. */
} vfs_open_flag_t;

typedef enum vfs_stat_flag
{
    VFS_S_IFDIR     = 0x4000,                       /**< Directory. */
    VFS_S_IFREG     = 0x8000,                       /**< Regular file. */
} vfs_stat_flag_t;

typedef enum vfs_seek_flag
{
    VFS_SEEK_SET    = 0,    /**< Start of file. */
    VFS_SEEK_CUR    = 1,    /**< Current position. */
    VFS_SEEK_END    = 2,    /**< End of file. */
} vfs_seek_flag_t;

typedef struct vfs_stat
{
    uint64_t st_mode;   /**< File mode. See #vfs_stat_flag_t. */
    uint64_t st_size;   /**< File size in bytes. */
    uint64_t st_mtime;  /**< File last modification time in seconds in UTC. */
} vfs_stat_t;

/**
 * @brief Callback function to list items in directory.
 * @param[in] name - Item name.
 * @param[in] stat - Item stat.
 * @param[in] data - user data which must be passed to the callback.
 * @return 0 on success, or non-zero to stop listing.
 */
typedef int (*vfs_ls_cb)(const char* name, const vfs_stat_t* stat, void* data);

typedef struct vfs_operations
{
    /**
     * @brief Destroy the filesystem object.
     *
     * It is guarantee that there are no other references to the filesystem,
     * and no more calls to the filesystem will be made.
     *
     * @param[in] thiz - This object.
     */
    void (*destroy)(struct vfs_operations* thiz);

    /**
     * @brief (Optional) List items in directory.
     * @param[in] thiz - This object.
     * @param[in] url - Path to the directory. Encoding in UTF-8.
     * @param[in] cb - Callback function.
     * @param[in] data - user data which must be passed to the callback.
     * @return 0 on success, or -errno on error.
     */
    int (*ls)(struct vfs_operations* thiz, const char* path, vfs_ls_cb fn, void* data);

    /**
     * @brief (Optional) Get file stat.
     * @param[in] thiz - This object.
     * @param[in] path - Path to file. Encoding in UTF-8.
     * @param[out] info - File stat.
     * @return - 0: on success.
     * @return - #VFS_ENOENT: No such file or directory.
     * @return - -errno: on error.
    */
    int (*stat)(struct vfs_operations* thiz, const char* path, vfs_stat_t* info);

    /**
     * @brief (Optional) Open file in binary mode.
     * @param[in] thiz - This object.
     * @param[out] fh - File handle.
     * @param[in] path - Path to file. Encoding in UTF-8.
     * @param[in] flags - Open flags. See #vfs_open_flag_t.
     * @return 0 on success, or -errno on error.
     */
    int (*open)(struct vfs_operations* thiz, uintptr_t* fh, const char* path, uint64_t flags);

    /**
     * @brief (Optional) Close file.
     * @param[in] thiz - This object.
     * @param[in] fh - File handle.
     * @return 0 on success, or -errno on error.
     */
    int (*close)(struct vfs_operations* thiz, uintptr_t fh);

    /**
     * @brief (Optional) Set the file position indicator for the stream pointed
     *   to by \p fh.
     * @param[in] thiz - This object.
     * @param[in] fh - File handle.
     * @param[in] offset - The new position measured in bytes.
     * @param[in] whence - Whence. Must one of #vfs_seek_flag_t.
     * @return - >=0: Resulting offset location as measured in  bytes from the
     *   beginning of the file.
     * @return - -errno: error.
    */
    int64_t (*seek)(struct vfs_operations* thiz, uintptr_t fh, int64_t offset, int whence);

    /**
     * @brief (Optional) Read data from file.
     *
     * The return value indicates the number of bytes that were actually read.
     * Value of 0 does not indicate end of file. To test for end of file, check
     * the return value against #VFS_EOF.
     *
     * @param[in] thiz - This object.
     * @param[in] fh - File handle.
     * @param[out] buf - Buffer to store data.
     * @param[in] size - Buffer size.
     * @return - >=0: Number of bytes read on success.
     * @return - #VFS_EOF: end of file.
     * @return - -errno: error.
     */
    int (*read)(struct vfs_operations* thiz, uintptr_t fh, void* buf, size_t len);

    /**
     * @brief (Optional) Write data to file.
     * @param[in] thiz - This object.
     * @param[in] fh - File handle.
     * @param[in] buf - Buffer containing data.
     * @param[in] size - Size of data.
     * @return - >=0: Number of bytes written on success.
     * @return - -errno: error.
     */
    int (*write)(struct vfs_operations* thiz, uintptr_t fh, const void* buf, size_t len);

    /**
     * @brief Create a directory.
     * @param[in] thiz - This object.
     * @param[in] path - Path to the directory. Encoding in UTF-8.
     * @return - 0: On success.
     * @return - #VFS_EEXIST: The directory already exists.
     * @return - -errno: On error.
     */
    int (*mkdir)(struct vfs_operations* thiz, const char* path);

    /**
     * @brief Remove a directory.
     * @param[in] thiz - This object.
     * @param[in] path - Path to the directory. Encoding in UTF-8.
     * @return - 0: On success.
     * @return - #VFS_ENOENT: No such file or directory.
     * @return - #VFS_NOTDIR: \p path is not a directory.
     * @return - -errno: On error.
     */
    int (*rmdir)(struct vfs_operations* thiz, const char* path);

    /**
     * @brief Remove a file.
     * @param[in] thiz - This object.
     * @param[in] path - Path to the file. Encoding in UTF-8.
     * @return - 0: On success.
     * @return - #VFS_ENOENT: No such file or directory.
     * @return - #VFS_EISDIR: \p path is a directory.
     * @return - -errno: On error.
     */
    int (*unlink)(struct vfs_operations* thiz, const char* path);
} vfs_operations_t;

/**
 * @brief Initialize the virtual file system.
 * @return 0 on success, or -errno on error.
 */
int vfs_init(void);

/**
 * @brief Exit the virtual file system.
 * @warning There must be no other function calls before doing this.
 */
void vfs_exit(void);

/**
 * @brief Mount a file system instance \p op into \p path.
 *
 * The \p path can be following styles:
 * 1. File style. like `/`, `/foo` or `/foo/bar/`.
 * 2. URL style. like `file:///`, `http://domain.com` or `ftp://username:password@domain.com/foo/`.
 *
 * This function does not check the \p path, so the following rules must obey:
 * 1. Always use slash '/', do not use backslash '\\'.
 * 2. Do not use continues slash unless it is part of URL scheme, e.g. `file:///`.
 * 3. File style path must start with '/'.
 *
 * @param[in] path - The path to mount.
 * @param[in] op - File system object.
 * @return 0 on success, or -errno on error.
 */
int vfs_mount(const char* path, vfs_operations_t* op);

/**
 * @brief Unmount path.
 * @param[in] path - The path to unmount.
 * @return 0 on success, or -errno on error.
 */
int vfs_unmount(const char* path);

/**
 * @brief Get global VFS visitor object.
 * @warning Do not destroy the returned object.
 * @return File system object.
 */
vfs_operations_t* vfs_visitor_instance(void);

#ifdef __cplusplus
}
#endif
#endif
