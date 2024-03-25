#ifndef __EV_MAP_H__
#define __EV_MAP_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup EV_UTILS_MAP Map
 * @ingroup EV_UTILS
 * @{
 */         

/**
 * @brief The node for map
 * @see eaf_map_t
 * @see EV_MAP_NODE_INIT
 */
typedef struct ev_map_node
{
    struct ev_map_node* __rb_parent_color;  /**< parent node | color */
    struct ev_map_node* rb_right;           /**< right node */
    struct ev_map_node* rb_left;            /**< left node */
} ev_map_node_t;

/**
 * @brief Static initializer for #ev_map_t
 * @see ev_map_t
 * @param[in] cmp   Compare function
 * @param[in] arg   Argument for compare function
 */
#define EV_MAP_INIT(cmp, arg)   { NULL, { cmp, arg }, 0 }

/**
 * @brief Static initializer for #ev_map_node_t
 * @see ev_map_node_t
 */
#define EV_MAP_NODE_INIT        { NULL, NULL, NULL }

/**
 * @brief Compare function.
 * @param key1  The key in the map
 * @param key2  The key user given
 * @param arg   User defined argument
 * @return      -1 if key1 < key2. 1 if key1 > key2. 0 if key1 == key2.
 */
typedef int(*ev_map_cmp_fn)(const ev_map_node_t* key1, const ev_map_node_t* key2, void* arg);

/**
 * @brief Map implemented as red-black tree
 * @see EV_MAP_INIT
 */
typedef struct ev_map
{
    ev_map_node_t*      rb_root;            /**< root node */

    struct
    {
        ev_map_cmp_fn   cmp;        /**< Pointer to compare function */
        void*           arg;        /**< User defined argument, which will passed to compare function */
    }cmp;                           /**< Compare function data */

    size_t              size;       /**< The number of nodes */
} ev_map_t;

/**
 * @brief Initialize the map referenced by handler.
 * @param handler   The pointer to the map
 * @param cmp       The compare function. Must not NULL
 * @param arg       User defined argument. Can be anything
 */
void vfs_map_init(ev_map_t* handler, ev_map_cmp_fn cmp, void* arg);

/**
 * @brief Insert the node into map.
 * @warning the node must not exist in any map.
 * @param handler   The pointer to the map
 * @param node      The node
 * @return          NULL if success, otherwise return the conflict node address.
 */
ev_map_node_t* vfs_map_insert(ev_map_t* handler, ev_map_node_t* node);

/**
 * @brief Delete the node from the map.
 * @warning The node must already in the map.
 * @param handler   The pointer to the map
 * @param node      The node
 */
void vfs_map_erase(ev_map_t* handler, ev_map_node_t* node);

/**
 * @brief Get the number of nodes in the map.
 * @param handler   The pointer to the map
 * @return          The number of nodes
 */
size_t vfs_map_size(const ev_map_t* handler);

/**
 * @brief Finds element with specific key
 * @param handler   The pointer to the map
 * @param key       The key
 * @return          An iterator point to the found node
 */
ev_map_node_t* vfs_map_find(const ev_map_t* handler,
    const ev_map_node_t* key);

/**
 * @brief Returns an iterator to the first element not less than the given key
 * @param handler   The pointer to the map
 * @param key       The key
 * @return          An iterator point to the found node
 */
ev_map_node_t* vfs_map_find_lower(const ev_map_t* handler,
    const ev_map_node_t* key);

/**
 * @brief Returns an iterator to the first element greater than the given key
 * @param handler   The pointer to the map
 * @param key       The key
 * @return          An iterator point to the found node
 */
ev_map_node_t* vfs_map_find_upper(const ev_map_t* handler,
    const ev_map_node_t* key);

/**
 * @brief Returns an iterator to the beginning
 * @param handler   The pointer to the map
 * @return          An iterator
 */
ev_map_node_t* vfs_map_begin(const ev_map_t* handler);

/**
 * @brief Returns an iterator to the end
 * @param handler   The pointer to the map
 * @return          An iterator
 */
ev_map_node_t* vfs_map_end(const ev_map_t* handler);

/**
 * @brief Get an iterator next to the given one.
 * @param node      Current iterator
 * @return          Next iterator
 */
ev_map_node_t* vfs_map_next(const ev_map_node_t* node);

/**
 * @brief Get an iterator before the given one.
 * @param node      Current iterator
 * @return          Previous iterator
 */
ev_map_node_t* vfs_map_prev(const ev_map_node_t* node);

/**
 * @} EV_UTILS/EV_UTILS_MAP
 */

#ifdef __cplusplus
}
#endif
#endif
