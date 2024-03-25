#ifndef __VFS_DEFINES_H__
#define __VFS_DEFINES_H__

#if defined(container_of)
#   define EV_CONTAINER_OF(ptr, type, member)   \
        container_of(ptr, type, member)
#elif defined(__GNUC__) || defined(__clang__)
#   define EV_CONTAINER_OF(ptr, type, member)   \
        ({ \
            const typeof(((type *)0)->member)*__mptr = (ptr); \
            (type *)((char *)__mptr - offsetof(type, member)); \
        })
#else
#   define EV_CONTAINER_OF(ptr, type, member)   \
        ((type *) ((char *) (ptr) - offsetof(type, member)))
#endif

#endif
