#ifndef __VFS_MUTEX_H__
#define __VFS_MUTEX_H__
#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
#include <windows.h>
typedef CRITICAL_SECTION vfs_mutex_t;
#else
#include <pthread.h>
typedef pthread_mutex_t vfs_mutex_t;
#endif

/**
 * @brief Initialize a mutex
 * @param[out] mutex - Mutex handle
 */
void vfs_mutex_init(vfs_mutex_t* mutex);

/**
 * @brief Destroy a mutex
 * @param[in] mutex - Mutex handle
 */
void vfs_mutex_exit(vfs_mutex_t* mutex);

/**
 * @brief Get mutex ownership
 * @param[in] mutex - Mutex handle
 */
void vfs_mutex_enter(vfs_mutex_t* mutex);

/**
 * @brief Release mutex ownership
 * @param[in] mutex - Mutex handle
 */
void vfs_mutex_leave(vfs_mutex_t* mutex);

#ifdef __cplusplus
}
#endif
#endif
