#include <stdlib.h>
#include <string.h>
#include "strlist.h"

vfs_strlist_t vfs_str_split(const vfs_str_t* str, const char* sep, int skip)
{
    vfs_strlist_t ret = VFS_STRLIST_INIT;
    size_t start = 0;
    const size_t sep_sz = strlen(sep);

    while (start < str->len)
    {
        /* Search for \p sep in substring [start, -1) */
        const char* start_ptr = str->str + start;
        const size_t left_sz = str->len - start;
        ptrdiff_t offset = vfs_str_search1(str, start, sep, sep_sz);

        /* Not found. */
        if (offset < 0)
        {
            vfs_strlist_append(&ret, start_ptr, left_sz);
            break;
        }

        /* Empty string */
        if ((size_t)offset == start)
        {
            if (!skip)
            {
                vfs_strlist_append(&ret, NULL, 0);
            }
            start += sep_sz;
            continue;
        }

        vfs_strlist_append(&ret, start_ptr, offset - start);
        start = offset + sep_sz;
    }

    return ret;
}

void vfs_strlist_exit(vfs_strlist_t* list)
{
    size_t i;
    for (i = 0; i < list->num; i++)
    {
        vfs_str_exit(&list->arr[i]);
    }

    free(list->arr);
    list->arr = NULL;
    list->num = 0;
}

void vfs_strlist_append(vfs_strlist_t* list, const char* data, size_t size)
{
    list->num += 1;
    vfs_str_t* new_arr = realloc(list->arr, sizeof(vfs_str_t) * list->num);
    if (new_arr == NULL)
    {
        abort();
    }
    list->arr = new_arr;

    vfs_str_t* str = &list->arr[list->num - 1];
    *str = vfs_str_from(data, size);
}

void vfs_strlist_append1(vfs_strlist_t* list, const char* data)
{
    size_t size = strlen(data);
    vfs_strlist_append(list, data, size);
}
