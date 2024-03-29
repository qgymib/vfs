#ifndef __VFS_ERRNO_H__
#define __VFS_ERRNO_H__

#include <errno.h>

#if EDOM > 0
#   define VFS__ERR(x)      (-(x))
#else
#   define VFS__ERR(x)      (x)
#endif

/**
 * @brief End of file.
 */
#define VFS_EOF             (-4096)

/**
 * @brief No such file or directory.
 */
#if defined(ENOENT)
#   define VFS_ENOENT       VFS__ERR(ENOENT)
#else
#   define VFS_ENOENT       (-2)
#endif

/**
 * @brief I/O error.
 */
#if defined(EIO)
#   define VFS_EIO			VFS__ERR(EIO)
#else
#   define VFS_EIO			(-5)
#endif

/**
 * @brief Out of memory.
 */
#if defined(ENOMEM)
#   define VFS_ENOMEM       VFS__ERR(ENOMEM)
#else
#   define VFS_ENOMEM       (-12)
#endif

/**
 * @brief Permission denied.
 */
#if defined(EACCES)
#   define VFS_EACCES       VFS__ERR(EACCES)
#else
#   define VFS_EACCES       (-13)
#endif

/**
 * @brief File exists.
 */
#if defined(EEXIST)
#   define VFS_EEXIST       VFS__ERR(EEXIST)
#else
#   define VFS_EEXIST       (-17)
#endif

/**
 * @brief Not a directory.
 */
#if defined(ENOTDIR)
#   define VFS_ENOTDIR      VFS__ERR(ENOTDIR)
#else
#   define VFS_ENOTDIR      (-20)
#endif

/**
 * @brief Is a directory.
 */
#if defined(EISDIR)
#   define VFS_EISDIR       VFS__ERR(EISDIR)
#else
#   define VFS_EISDIR       (-21)
#endif

/**
 * @brief Invalid argument.
 */
#if defined(EINVAL)
#   define VFS_EINVAL       VFS__ERR(EINVAL)
#else
#   define VFS_EINVAL       (-22)
#endif

#if defined(ENOSYS)
#   define VFS_ENOSYS       VFS__ERR(ENOSYS)
#else
#   define VFS_ENOSYS       (-40)
#endif

/**
 * @brief Directory not empty.
 */
#if defined(ENOTEMPTY)
#   define VFS_ENOTEMPTY    VFS__ERR(ENOTEMPTY)
#else
#   define VFS_ENOTEMPTY    (-41)
#endif

#endif
