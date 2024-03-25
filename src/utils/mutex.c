#include "mutex.h"

#if defined(_WIN32)

void vfs_mutex_init(vfs_mutex_t* mutex)
{
    InitializeCriticalSection(mutex);
}

void vfs_mutex_exit(vfs_mutex_t* mutex)
{
    DeleteCriticalSection(mutex);
}

void vfs_mutex_enter(vfs_mutex_t* mutex)
{
    EnterCriticalSection(mutex);
}

void vfs_mutex_leave(vfs_mutex_t* mutex)
{
    LeaveCriticalSection(mutex);
}

#else

#include <stdlib.h>

void vfs_mutex_init(vfs_mutex_t* mutex)
{
#if defined(NDEBUG) || !defined(PTHREAD_MUTEX_ERRORCHECK)
    if (pthread_mutex_init(mutex, NULL) != 0)
    {
        abort();
    }
#else
    pthread_mutexattr_t attr;
    if (pthread_mutexattr_init(&attr))
    {
        abort();
    }

    if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK))
    {
        abort();
    }

    if (pthread_mutex_init(mutex, &attr) != 0)
    {
        abort();
    }

    if (pthread_mutexattr_destroy(&attr))
    {
        abort();
    }
#endif
}

void vfs_mutex_exit(vfs_mutex_t* mutex)
{
    if (pthread_mutex_destroy(mutex) != 0)
    {
        abort();
    }
}

void vfs_mutex_enter(vfs_mutex_t* mutex)
{
    if (pthread_mutex_lock(mutex) != 0)
    {
        abort();
    }
}

void vfs_mutex_leave(vfs_mutex_t* mutex)
{
    if (pthread_mutex_unlock(mutex) != 0)
    {
        abort();
    }
}

#endif
