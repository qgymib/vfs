#include "rwlock.h"

void vfs_rwlock_init(vfs_rwlock_t* lock)
{
#if defined(_WIN32)
	InitializeSRWLock(lock);
#else
	pthread_rwlock_init(lock, NULL);
#endif
}

void vfs_rwlock_exit(vfs_rwlock_t* lock)
{
#if defined(_WIN32)
	(void)lock;
#else
	pthread_rwlock_destroy(lock);
#endif
}

void vfs_rwlock_rdlock(vfs_rwlock_t* lock)
{
#if defined(_WIN32)
	AcquireSRWLockShared(lock);
#else
	pthread_rwlock_rdlock(lock);
#endif
}

void vfs_rwlock_rdunlock(vfs_rwlock_t* lock)
{
#if defined(_WIN32)
	ReleaseSRWLockShared(lock);
#else
	pthread_rwlock_unlock(lock);
#endif
}

void vfs_rwlock_wrlock(vfs_rwlock_t* lock)
{
#if defined(_WIN32)
	AcquireSRWLockExclusive(lock);
#else
	pthread_rwlock_wrlock(lock);
#endif
}

void vfs_rwlock_wrunlock(vfs_rwlock_t* lock)
{
#if defined(_WIN32)
	ReleaseSRWLockExclusive(lock);
#else
	pthread_rwlock_unlock(lock);
#endif
}
