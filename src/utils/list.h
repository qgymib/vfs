#ifndef __EV_LIST_H__
#define __EV_LIST_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup EV_UTILS_LIST List
 * @ingroup EV_UTILS
 * @{
 */

/**
 * @brief Static initializer for #ev_list_t
 * @see ev_list_t
 */
#define EV_LIST_INIT            { NULL, NULL, 0 }

/**
 * @brief Static initializer for #ev_list_node_t
 * @see ev_list_node_t
 */
#define EV_LIST_NODE_INIT       { NULL, NULL }

/**
 * @brief The list node.
 * This node must put in your struct.
 * @see EV_LIST_NODE_INIT
 */
typedef struct ev_list_node
{
    struct ev_list_node*    p_after;    /**< Pointer to next node */
    struct ev_list_node*    p_before;   /**< Pointer to previous node */
}ev_list_node_t;

/**
 * @brief Double Linked List
 * @see EV_LIST_INIT
 */
typedef struct ev_list
{
    ev_list_node_t*         head;       /**< Pointer to HEAD node */
    ev_list_node_t*         tail;       /**< Pointer to TAIL node */
    size_t                  size;       /**< The number of total nodes */
}ev_list_t;

/**
 * @brief Initialize Double Linked List.
 * @note It is guarantee that memset() to zero have the same affect.
 * @param[out] handler  Pointer to list
 */
void vfs_list_init(ev_list_t* handler);

/**
 * @brief Insert a node to the head of the list.
 * @warning the node must not exist in any list.
 * @param[in,out] handler   Pointer to list
 * @param[in,out] node      Pointer to a new node
 */
void vfs_list_push_front(ev_list_t* handler, ev_list_node_t* node);

/**
 * @brief Insert a node to the tail of the list.
 * @warning the node must not exist in any list.
 * @param[in,out] handler   Pointer to list
 * @param[in,out] node      Pointer to a new node
 */
void vfs_list_push_back(ev_list_t* handler, ev_list_node_t* node);

/**
 * @brief Insert a node in front of a given node.
 * @warning the node must not exist in any list.
 * @param[in,out] handler   Pointer to list
 * @param[in,out] pos       Pointer to a exist node
 * @param[in,out] node      Pointer to a new node
 */
void vfs_list_insert_before(ev_list_t* handler, ev_list_node_t* pos, ev_list_node_t* node);

/**
 * @brief Insert a node right after a given node.
 * @warning the node must not exist in any list.
 * @param[in,out] handler   Pointer to list
 * @param[in,out] pos       Pointer to a exist node
 * @param[in,out] node      Pointer to a new node
 */
void vfs_list_insert_after(ev_list_t* handler, ev_list_node_t* pos, ev_list_node_t* node);

/**
 * @brief Delete a exist node
 * @warning The node must already in the list.
 * @param[in,out] handler   Pointer to list
 * @param[in,out] node      The node you want to delete
 */
void vfs_list_erase(ev_list_t* handler, ev_list_node_t* node);

/**
 * @brief Get the number of nodes in the list.
 * @param[in] handler   Pointer to list
 * @return              The number of nodes
 */
size_t vfs_list_size(const ev_list_t* handler);

/**
 * @brief Get the first node and remove it from the list.
 * @param[in,out] handler   Pointer to list
 * @return                  The first node
 */
ev_list_node_t* vfs_list_pop_front(ev_list_t* handler);

/**
 * @brief Get the last node and remove it from the list.
 * @param[in,out] handler   Pointer to list
 * @return                  The last node
 */
ev_list_node_t* vfs_list_pop_back(ev_list_t* handler);

/**
 * @brief Get the first node.
 * @param[in] handler   Pointer to list
 * @return              The first node
 */
ev_list_node_t* vfs_list_begin(const ev_list_t* handler);

/**
 * @brief Get the last node.
 * @param[in] handler   The handler of list
 * @return              The last node
 */
ev_list_node_t* vfs_list_end(const ev_list_t* handler);

/**
* @brief Get next node.
* @param[in] node   Current node
* @return           The next node
*/
ev_list_node_t* vfs_list_next(const ev_list_node_t* node);

/**
 * @brief Get previous node.
 * @param[in] node  current node
 * @return          previous node
 */
ev_list_node_t* vfs_list_prev(const ev_list_node_t* node);

/**
 * @brief Move all elements from \p src into the end of \p dst.
 * @param[in] dst   Destination list.
 * @param[in] src   Source list.
 */
void vfs_list_migrate(ev_list_t* dst, ev_list_t* src);

/**
 * @} EV_UTILS/EV_UTILS_LIST
 */

#ifdef __cplusplus
}
#endif
#endif
