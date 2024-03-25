#ifndef __VFS_THREAD_H__
#define __VFS_THREAD_H__
#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
#include <windows.h>
typedef HANDLE vfs_thread_t;
#else
#include <pthread.h>
typedef pthread_t vfs_thread_t;
#endif

/**
 * @brief Thread callback
 * @param[in] arg - Callback argument
 */
typedef void (*vfs_thread_cb)(void* arg);

/**
 * @brief Initialize a thread
 * @param[out] thr - Thread handle
 * @param[in] cb - Thread callback
 * @param[in] arg - Callback argument
 */
void vfs_thread_init(vfs_thread_t* thr, vfs_thread_cb cb, void* arg);

/**
 * @brief Destroy a thread
 * @param[in] thr - Thread handle
 */
void vfs_thread_exit(vfs_thread_t thr);

#ifdef __cplusplus
}
#endif
#endif
