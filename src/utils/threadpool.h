#ifndef __VFS_THREAD_POOL_H__
#define __VFS_THREAD_POOL_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vfs_threadpool vfs_threadpool_t;

typedef struct vfs_threadpool_cfg
{
	size_t number_of_thread;	/**< Number of threads. */
	size_t queue_capacity;		/**< Queue capacity. */
} vfs_threadpool_cfg_t;

/**
 * @brief Work callback
 * @param[in] idx - Thread index
 * @param[in] status - Work status
 * @param[in] data - Work data
 */
typedef void (*vfs_threadpool_work_cb)(int status, void* data);

/**
 * @brief Initialize a thread pool
 * @param[out] pool - Thread pool handle
 * @param[in] cfg - Thread pool configuration
 */
void vfs_threadpool_init(vfs_threadpool_t** pool, const vfs_threadpool_cfg_t* cfg);

/**
 * @brief Destroy a thread pool
 * @param[in] pool - Thread pool handle
 */
void vfs_threadpool_exit(vfs_threadpool_t* pool);

/**
 * @brief Submit a work
 * @param[in] pool - Thread pool handle
 * @param[in] idx - Thread index
 * @param[in] cb - Work callback
 * @param[in] data - Work data
 */
int vfs_threadpool_submit(vfs_threadpool_t* pool, size_t idx, vfs_threadpool_work_cb cb, void* data);

#ifdef __cplusplus
}
#endif
#endif
