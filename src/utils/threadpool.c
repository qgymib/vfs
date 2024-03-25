#include <stdlib.h>
#include <errno.h>
#include "threadpool.h"
#include "thread.h"
#include "list.h"
#include "mutex.h"
#include "sem.h"
#include "defs.h"

typedef struct vfs_threadpool_job
{
	ev_list_node_t			node;

	vfs_threadpool_work_cb	cb;
	void*					data;
} vfs_threadpool_job_t;

typedef struct vfs_threadpool_worker
{
	vfs_threadpool_t*		belong;

	ev_list_t				job_queue;
	vfs_mutex_t				job_queue_lock;
	vfs_sem_t				job_queue_sem;
	size_t					job_queue_cap;

	size_t					idx;
	vfs_thread_t			worker;
} vfs_threadpool_worker_t;

struct vfs_threadpool
{
	int						flag_running;	/**< Running flag. */
	size_t					worker_sz;		/**< Number of threads. */

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning( disable : 4200 )
#endif
	vfs_threadpool_worker_t	workers[];		/**< Thread handle. */
#if defined(_MSC_VER)
#pragma warning( pop )
#endif
};

static void _vfs_threadpool_worker_do_job(vfs_threadpool_job_t* job, int status)
{
	job->cb(status, job->data);
}

static void _vfs_threadpool_worker_process_all_job(vfs_threadpool_worker_t* worker, int status)
{
	while (1)
	{
		vfs_threadpool_job_t* job = NULL;
		vfs_mutex_enter(&worker->job_queue_lock);
		{
			ev_list_node_t* it = vfs_list_pop_front(&worker->job_queue);
			if (it != NULL)
			{
				job = EV_CONTAINER_OF(it, vfs_threadpool_job_t, node);
			}
		}
		vfs_mutex_leave(&worker->job_queue_lock);

		if (job == NULL)
		{
			break;
		}

		_vfs_threadpool_worker_do_job(job, status);
		free(job);
	}
}

static void _vfs_threadpool_worker(void* arg)
{
	vfs_threadpool_worker_t* worker = arg;
	vfs_threadpool_t* pool = worker->belong;

	while (pool->flag_running)
	{
		vfs_sem_wait(&worker->job_queue_sem);

		/* Process all jobs. */
		_vfs_threadpool_worker_process_all_job(worker, 0);
	}

	_vfs_threadpool_worker_process_all_job(worker, -ECANCELED);
}

static void _vfs_threadpool_init_worker(vfs_threadpool_t* pool, vfs_threadpool_worker_t* worker, size_t idx, size_t cap)
{
	worker->belong = pool;

	vfs_list_init(&worker->job_queue);
	vfs_mutex_init(&worker->job_queue_lock);
	vfs_sem_init(&worker->job_queue_sem, 0);
	worker->job_queue_cap = cap;

	worker->idx = idx;
	vfs_thread_init(&worker->worker, _vfs_threadpool_worker, worker);
}

void vfs_threadpool_init(vfs_threadpool_t** pool, const vfs_threadpool_cfg_t* cfg)
{
	size_t malloc_sz = sizeof(vfs_threadpool_t) + sizeof(vfs_threadpool_worker_t) * cfg->number_of_thread;
	vfs_threadpool_t* tmp_pool = malloc(malloc_sz);
	if (tmp_pool == NULL)
	{
		abort();
	}
	tmp_pool->flag_running = 1;
	tmp_pool->worker_sz = cfg->number_of_thread;

	size_t i;
	for (i = 0; i < tmp_pool->worker_sz; i++)
	{
		_vfs_threadpool_init_worker(tmp_pool, &tmp_pool->workers[i], i, cfg->queue_capacity);
	}

	*pool = tmp_pool;
}

void vfs_threadpool_exit(vfs_threadpool_t* pool)
{
	pool->flag_running = 0;

	size_t i;
	for (i = 0; i < pool->worker_sz; i++)
	{
		vfs_threadpool_worker_t* worker = &pool->workers[i];
		vfs_sem_post(&worker->job_queue_sem);

		vfs_thread_exit(worker->worker);
		vfs_sem_exit(&worker->job_queue_sem);
		vfs_mutex_exit(&worker->job_queue_lock);
	}

	free(pool);
}

int vfs_threadpool_submit(vfs_threadpool_t* pool, size_t idx, vfs_threadpool_work_cb cb, void* data)
{
	if (idx > pool->worker_sz)
	{
		return -EINVAL;
	}
	vfs_threadpool_worker_t* worker = &pool->workers[idx];

	vfs_threadpool_job_t* job = malloc(sizeof(vfs_threadpool_job_t));
	if (job == NULL)
	{
		return -ENOMEM;
	}
	job->cb = cb;
	job->data = data;

	int ret = 0;
	vfs_mutex_enter(&worker->job_queue_lock);
	do {
		if (vfs_list_size(&worker->job_queue) > worker->job_queue_cap)
		{
			ret = -ENOBUFS;
			break;
		}

		vfs_list_push_back(&worker->job_queue, &job->node);
	} while (0);
	vfs_mutex_leave(&worker->job_queue_lock);

	if (ret != 0)
	{
		free(job);
		return ret;
	}

	vfs_sem_post(&worker->job_queue_sem);

	return 0;
}
