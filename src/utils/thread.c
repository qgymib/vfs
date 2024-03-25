#include "thread.h"

#if defined(_WIN32)

#include <process.h>

typedef struct vfs_thread_helper_win
{
    vfs_thread_cb   cb;         /**< User thread body */
    void*           arg;        /**< User thread argument */
    HANDLE          start_sem;  /**< Start semaphore */
    HANDLE          thread_id;  /**< Thread handle */
}vfs_thread_helper_win_t;

static unsigned __stdcall _ev_thread_proxy_proc_win(void* lpThreadParameter)
{
    vfs_thread_helper_win_t* helper = lpThreadParameter;
    vfs_thread_cb cb = helper->cb;
    void* arg = helper->arg;

    if (!ReleaseSemaphore(helper->start_sem, 1, NULL))
    {
        abort();
    }
    cb(arg);

    return 0;
}

void vfs_thread_init(vfs_thread_t* thr, vfs_thread_cb cb, void* arg)
{
    vfs_thread_helper_win_t helper = { cb, arg, NULL, NULL };

    if ((helper.start_sem = CreateSemaphore(NULL, 0, 1, NULL)) == NULL)
    {
        abort();
    }

    helper.thread_id = (HANDLE)_beginthreadex(NULL, 0, _ev_thread_proxy_proc_win, &helper, CREATE_SUSPENDED, NULL);
    if (helper.thread_id == NULL)
    {
        abort();
    }

    if (ResumeThread(helper.thread_id) == -1)
    {
        abort();
    }

    if (WaitForSingleObject(helper.start_sem, INFINITE) != WAIT_OBJECT_0)
    {
        abort();
    }

    *thr = helper.thread_id;
    CloseHandle(helper.start_sem);
}

void vfs_thread_exit(vfs_thread_t thr)
{
    WaitForSingleObject(thr, INFINITE);
    CloseHandle(thr);
}

#else

#include <semaphore.h>
#include <stdlib.h>
#include <errno.h>

typedef struct vfs_thread_helper_unix
{
    vfs_thread_cb   cb;
    void*           arg;
    sem_t           sem;
}vfs_thread_helper_unix_t;

static void* _ev_thread_proxy_unix(void* param)
{
    vfs_thread_helper_unix_t* helper = param;
    vfs_thread_cb cb = helper->cb;
    void* arg = helper->arg;

    sem_post(&helper->sem);

    cb(arg);
    return NULL;
}

void vfs_thread_init(vfs_thread_t* thr, vfs_thread_cb cb, void* arg)
{
    vfs_thread_helper_unix_t helper;
    helper.cb = cb;
    helper.arg = arg;
    if (sem_init(&helper.sem, 0, 0) != 0)
    {
        abort();
    }

    if (pthread_create(thr, NULL, _ev_thread_proxy_unix, &helper) != 0)
    {
        abort();
    }

    int err;
    do
    {
        err = sem_wait(&helper.sem);
    } while (err == -1 && errno == EINTR);

    if (err != 0)
    {
        abort();
    }

    sem_destroy(&helper.sem);
}

void vfs_thread_exit(vfs_thread_t thr)
{
    pthread_join(thr, NULL);
}

#endif
