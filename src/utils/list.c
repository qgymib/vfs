#include <string.h>
#include "list.h"

static void _list_lite_set_once(ev_list_t* handler, ev_list_node_t* node)
{
    handler->head = node;
    handler->tail = node;
    node->p_after = NULL;
    node->p_before = NULL;
    handler->size = 1;
}

void vfs_list_init(ev_list_t* handler)
{
    memset(handler, 0, sizeof(*handler));
}

void vfs_list_push_back(ev_list_t* handler, ev_list_node_t* node)
{
    if (handler->head == NULL)
    {
        _list_lite_set_once(handler, node);
        return;
    }

    node->p_after = NULL;
    node->p_before = handler->tail;
    handler->tail->p_after = node;
    handler->tail = node;
    handler->size++;
}

void vfs_list_insert_before(ev_list_t* handler, ev_list_node_t* pos, ev_list_node_t* node)
{
    if (handler->head == pos)
    {
        vfs_list_push_front(handler, node);
        return;
    }

    node->p_before = pos->p_before;
    node->p_after = pos;
    pos->p_before->p_after = node;
    pos->p_before = node;
    handler->size++;
}

void vfs_list_insert_after(ev_list_t* handler,  ev_list_node_t* pos, ev_list_node_t* node)
{
    if (handler->tail == pos)
    {
        vfs_list_push_back(handler, node);
        return;
    }

    node->p_before = pos;
    node->p_after = pos->p_after;
    pos->p_after->p_before = node;
    pos->p_after = node;
    handler->size++;
}

void vfs_list_push_front(ev_list_t* handler, ev_list_node_t* node)
{
    if (handler->head == NULL)
    {
        _list_lite_set_once(handler, node);
        return;
    }

    node->p_before = NULL;
    node->p_after = handler->head;
    handler->head->p_before = node;
    handler->head = node;
    handler->size++;
}

ev_list_node_t* vfs_list_begin(const ev_list_t* handler)
{
    return handler->head;
}

ev_list_node_t* vfs_list_end(const ev_list_t* handler)
{
    return handler->tail;
}

ev_list_node_t* vfs_list_next(const ev_list_node_t* node)
{
    return node->p_after;
}

ev_list_node_t* vfs_list_prev(const ev_list_node_t* node)
{
    return node->p_before;
}

void vfs_list_erase(ev_list_t* handler, ev_list_node_t* node)
{
    handler->size--;

    /* Only one node */
    if (handler->head == node && handler->tail == node)
    {
        handler->head = NULL;
        handler->tail = NULL;
        goto fin;
    }

    if (handler->head == node)
    {
        node->p_after->p_before = NULL;
        handler->head = node->p_after;
        goto fin;
    }

    if (handler->tail == node)
    {
        node->p_before->p_after = NULL;
        handler->tail = node->p_before;
        goto fin;
    }

    node->p_before->p_after = node->p_after;
    node->p_after->p_before = node->p_before;

fin:
    node->p_after = NULL;
    node->p_before = NULL;
}

ev_list_node_t* vfs_list_pop_front(ev_list_t* handler)
{
    ev_list_node_t* node = handler->head;
    if (node == NULL)
    {
        return NULL;
    }

    vfs_list_erase(handler, node);
    return node;
}

ev_list_node_t* vfs_list_pop_back(ev_list_t* handler)
{
    ev_list_node_t* node = handler->tail;
    if (node == NULL)
    {
        return NULL;
    }

    vfs_list_erase(handler, node);
    return node;
}

size_t vfs_list_size(const ev_list_t* handler)
{
    return handler->size;
}

void vfs_list_migrate(ev_list_t* dst, ev_list_t* src)
{
    if (src->head == NULL)
    {
        return;
    }

    if (dst->tail == NULL)
    {
        *dst = *src;
    }
    else
    {
        dst->tail->p_after = src->head;
        dst->tail->p_after->p_before = dst->tail;
        dst->tail = src->tail;
        dst->size += src->size;
    }

    src->head = NULL;
    src->tail = NULL;
    src->size = 0;
}
