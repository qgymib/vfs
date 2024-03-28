#ifndef __VFS_DEFINES_H__
#define __VFS_DEFINES_H__

/**
 * @brief Get the container of a pointer
 * @param[in] ptr - Pointer
 * @param[in] type - Container Type
 * @param[in] member - Member
 * @return The address of the container for the pointer.
*/
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

/**
 * @brief Get the minimum of two values.
 * @param[in] a - First value.
 * @param[in] b - Second value.
 * @return The minimum value.
 */
#ifndef min
#   define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

/**
 * @brief Get the maximum of two values.
 * @param[in] a - First value.
 * @param[in] b - Second value.
 * @return The maximum value.
 */
#ifndef max
#   define max(a, b) (((a) < (b)) ? (b) : (a))
#endif

#endif
