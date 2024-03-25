#ifndef __VFS_H__
#define __VFS_H__

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum vfs_open_flag
{
    VFS_O_RDONLY    = 0x0001,                       /**< Read only. */
    VFS_O_WRONLY    = 0x0002,                       /**< Write only. */
    VFS_O_RDWR      = VFS_O_RDONLY | VFS_O_WRONLY,  /**< Read and write. */
    VFS_O_APPEND    = 0x0004,                       /**< Append to file. */
    VFS_O_TRUNCATE  = 0x0008,                       /**< Truncate file to zero. */
    VFS_O_CREATE    = 0x0010,                       /**< Create file if not exist. */
} vfs_open_flag_t;

typedef enum vfs_stat_flag
{
    VFS_S_IFDIR     = 0x4000,                       /**< Directory. */
    VFS_S_IFREG     = 0x8000,                       /**< Regular file. */
} vfs_stat_flag_t;

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
     * @brief (Optional) Read data from file.
     * @param[in] thiz - This object.
     * @param[in] fh - File handle.
     * @param[out] buf - Buffer to store data.
     * @param[in] size - Buffer size.
     * @return Number of bytes read on success, or -errno on error.
     */
    int (*read)(struct vfs_operations* thiz, uintptr_t fh, void* buf, size_t len);

    /**
     * @brief (Optional) Write data to file.
     * @param[in] thiz - This object.
     * @param[in] fh - File handle.
     * @param[in] buf - Buffer containing data.
     * @param[in] size - Size of data.
     * @return Number of bytes written on success, or -errno on error.
     */
    int (*write)(struct vfs_operations* thiz, uintptr_t fh, const void* buf, size_t len);
} vfs_operations_t;

/**
 * @brief Initialize the virtual file system.
 * @return 0 on success, or -errno on error.
 */
int vfs_init(void);

/**
 * @brief Exit the virtual file system.
 */
void vfs_exit(void);

/**
 * @brief Mount a file system instance \p op into \p path.
 * @param[in] path - The path to mount. The path must be absolute, and does not
 *   contains duplicate slashes.
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
 * @brief Get visitor file system object.
 * @return File system object.
 */
vfs_operations_t* vfs_visitor(void);

#ifdef __cplusplus
}
#endif
#endif
