#ifndef __VFS_UTILS_RWLOCK_H__
#define __VFS_UTILS_RWLOCK_H__

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
#	include <windows.h>
typedef SRWLOCK vfs_rwlock_t;
#else
#	include <pthread.h>
typedef pthread_rwlock_t vfs_rwlock_t;
#endif

/**
 * @brief Initialize a read-write lock.
 * @param[out] lock - Read-write lock
 */
void vfs_rwlock_init(vfs_rwlock_t* lock);

/**
 * @brief Destroy a read-write lock.
 * @param[in,out] lock - Read-write lock
 */
void vfs_rwlock_exit(vfs_rwlock_t* lock);

/**
 * @brief Get read lock ownership.
 * @param[in,out] lock - Read-write lock
 */
void vfs_rwlock_rdlock(vfs_rwlock_t* lock);

/**
 * @brief Release read lock ownership.
 * @param[in,out] lock - Read-write lock
*/
void vfs_rwlock_rdunlock(vfs_rwlock_t* lock);

/**
 * @brief Get write lock ownership.
 * @param[in,out] lock - Read-write lock
 */
void vfs_rwlock_wrlock(vfs_rwlock_t* lock);

/**
 * @brief Release write lock ownership.
 * @param[in,out] lock - Read-write lock
 */
void vfs_rwlock_wrunlock(vfs_rwlock_t* lock);

#ifdef __cplusplus
}
#endif
#endif
