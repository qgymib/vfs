#include <stdlib.h>
#include <errno.h>
#include "sem.h"

#if defined(_WIN32)

void vfs_sem_init(vfs_sem_t* sem, unsigned val)
{
    if ((*sem = CreateSemaphore(NULL, val, INT_MAX, NULL)) == NULL)
    {
        abort();
    }
}

void vfs_sem_exit(vfs_sem_t* sem)
{
    CloseHandle(sem);
}

void vfs_sem_post(vfs_sem_t* sem)
{
    if (ReleaseSemaphore(sem, 1, NULL) == 0)
    {
        abort();
    }
}

void vfs_sem_wait(vfs_sem_t* sem)
{
    if (WaitForSingleObject(sem, INFINITE) != WAIT_OBJECT_0)
    {
        abort();
    }
}

#else

void vfs_sem_init(vfs_sem_t* sem, unsigned val)
{
    if (sem_init(sem, 0, val) != 0)
    {
        abort();
    }
}

void vfs_sem_exit(vfs_sem_t* sem)
{
    if (sem_destroy(sem) != 0)
    {
        abort();
    }
}

void vfs_sem_post(vfs_sem_t* sem)
{
    if (sem_post(sem) != 0)
    {
        abort();
    }
}

void vfs_sem_wait(vfs_sem_t* sem)
{
    int r;
    do
    {
        r = sem_wait(sem);
    } while (r == -1 && errno == EINTR);

    if (r)
    {
        abort();
    }
}

#endif
