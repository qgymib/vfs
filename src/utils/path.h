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
 * @brief Convert \p str to native path.
 * @param[in,out] str - String object.
 */
void vfs_path_to_native(vfs_str_t* str);

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

#ifdef __cplusplus
}
#endif
#endif
