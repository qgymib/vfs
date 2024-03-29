#ifndef __VFS_STRING_LIST_H__
#define __VFS_STRING_LIST_H__

#include "str.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vfs_strlist
{
    vfs_str_t*  arr;    /**< Array of strings. */
    size_t      num;    /**< Number of strings. */
} vfs_strlist_t;

/**
 * @brief Static initializer for #vfs_strlist_t.
 * Any instance of #vfs_strlist_t must always initialized to it.
 */
#define VFS_STRLIST_INIT    { NULL, 0 }

/**
 * @brief Split the string \p str wherever \p sep occurs, and return the list
 *   of those strings.
 * @param[in] str - String to split.
 * @param[in] sep - Separator.
 * @param[in] skip - 0 if keep the empty field in the list, 1 if skip empty field.
 * @return List of strings.
 */
vfs_strlist_t vfs_str_split(const vfs_str_t* str, const char* sep, int skip);

/**
 * @brief Free the list of strings.
 * @param[in,out] list - List of strings.
 */
void vfs_strlist_exit(vfs_strlist_t* list);

/**
 * @brief Append \p data and \p size to the list.
 * @param[in,out] list - List of strings.
 * @param[in] data - Data to append.
 * @param[in] size - Size in bytes.
 */
void vfs_strlist_append(vfs_strlist_t* list, const char* data, size_t size);

/**
 * @brief Append \p data to the list.
 * @param[in,out] list - List of strings.
 * @param[in] data - Data to append.
 */
void vfs_strlist_append1(vfs_strlist_t* list, const char* data);

#ifdef __cplusplus
}
#endif
#endif
