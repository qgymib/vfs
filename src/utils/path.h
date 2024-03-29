#ifndef __VFS_PATH_H__
#define __VFS_PATH_H__

#include "utils/str.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get parent path of \p path.
 * @param[in] path - The path. It must start with '/' and not have ending '/'.
 * @return The parent path. If no parent path, return an empty string.
 */
vfs_str_t vfs_path_parent(const vfs_str_t* path, vfs_str_t* basename);

/**
 * @brief Convert \p str to native style path.
 * @param[in,out] str - String object.
 */
void vfs_path_to_native(vfs_str_t* str);

/**
 * @brief Convert \p str to unix style path.
 * @param[in,out] str - String object.
 */
void vfs_path_to_unix(vfs_str_t* str);

/**
 * @brief Convert \p str to win32 style path.
 * @param[in,out] str - String object.
 */
void vfs_path_to_win32(vfs_str_t* str);

/**
 * @brief Get layer path of \p path.
 *
 * For the path `/foo/bar`, the layer is:
 * [0]: `/`
 * [1]: `/foo`
 * [2]: `/foo/bar`
 *
 * @param[in] path - The path. It must start with '/' and not have ending '/'.
 * @param[in] level - The layer.
 * @return The layer path.
 */
vfs_str_t vfs_path_layer(const vfs_str_t* path, size_t level);

/**
 * @brief Check if \p path is a native root.
 * @param[in] path - The path.
 * @return true if \p path is a native root.
 */
int vfs_path_is_native_root(const vfs_str_t* path);

/**
 * @brief Check if \p path is a root.
 * @param[in] path - The path.
 * @return true if \p path is a root.
 */
int vfs_path_is_root(const vfs_str_t* path);

#ifdef __cplusplus
}
#endif
#endif
