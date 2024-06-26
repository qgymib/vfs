#ifndef __VFS_ERROR_CODE_H__
#define __VFS_ERROR_CODE_H__
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Translate system error code to vfs error code.
 * @param[in] errcode - System error code.
 * @return vfs error code.
 */
int vfs_translate_sys_err(int errcode);

/**
 * @brief Translate posix error code to VFS error code.
 * @param[in] errcode - Posix error code.
 * @return VFS error code.
 */
int vfs_translate_posix_error(int errcode);

#ifdef __cplusplus
}
#endif
#endif
