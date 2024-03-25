#ifndef __VFS_SEM_H__
#define __VFS_SEM_H__
#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
#include <windows.h>
typedef HANDLE vfs_sem_t;
#else
#include <semaphore.h>
typedef sem_t vfs_sem_t;
#endif

/**
 * @brief Initialize a semaphore
 * @param[out] sem - Semaphore handle
 * @param[in] val - Initial semaphore value
 */
void vfs_sem_init(vfs_sem_t* sem, unsigned val);

/**
 * @brief Destroy a semaphore
 * @param[in] sem - Semaphore handle
 */
void vfs_sem_exit(vfs_sem_t* sem);

/**
 * @brief Get semaphore ownership
 * @param[in] sem - Semaphore handle
 */
void vfs_sem_post(vfs_sem_t* sem);

/**
 * @brief Wait for semaphore
 * @param[in] sem - Semaphore handle
 */
void vfs_sem_wait(vfs_sem_t* sem);

#ifdef __cplusplus
}
#endif
#endif
