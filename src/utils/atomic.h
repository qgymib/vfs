#ifndef __VFS_UTILS_ATOMIC_H__
#define __VFS_UTILS_ATOMIC_H__
#ifdef __cplusplus
extern "C" {
#endif

#if __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_ATOMICS__)

//////////////////////////////////////////////////////////////////////////
// C11 support _Atomic
//////////////////////////////////////////////////////////////////////////
#include <stdatomic.h>

typedef atomic_int vfs_atomic_t;
#define vfs_atomic_add(a) (atomic_fetch_add(a, 1) + 1)
#define vfs_atomic_dec(a) (atomic_fetch_add(a, -1) - 1)

typedef atomic_int_fast64_t vfs_atomic64_t;
#define vfs_atomic64_add(a) vfs_atomic_add(a)
#define vfs_atomic64_dec(a) vfs_atomic_dec(a)

#elif defined(_WIN32)

//////////////////////////////////////////////////////////////////////////
// Windows
//////////////////////////////////////////////////////////////////////////

#include <windows.h>

typedef LONG vfs_atomic_t;
#define vfs_atomic_add(a) InterlockedIncrement(a)
#define vfs_atomic_dec(a) InterlockedDecrement(a)

typedef LONG64 vfs_atomic64_t;
#define vfs_atomic64_add(a) InterlockedIncrement64(a)
#define vfs_atomic64_dec(a) InterlockedDecrement64(a)

#elif defined(__GNUC__) || defined(__clang__)

//////////////////////////////////////////////////////////////////////////
// GCC or Clang
//////////////////////////////////////////////////////////////////////////

#include <stdint.h>

typedef int vfs_atomic_t;
#define vfs_atomic_add(a) __atomic_add_fetch(a, 1, __ATOMIC_SEQ_CST)
#define vfs_atomic_dec(a) __atomic_sub_fetch(a, 1, __ATOMIC_SEQ_CST)

typedef int64_t vfs_atomic64_t;
#define vfs_atomic64_add(a) vfs_atomic_add(a)
#define vfs_atomic64_dec(a) vfs_atomic_dec(a)

#else

#error "unsupport platform"

#endif

#ifdef __cplusplus
}
#endif
#endif
