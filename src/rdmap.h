/*
 * librdkafka - The Apache Kafka C/C++ library
 *
 * Copyright (c) 2020 Magnus Edenhill
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _RDMAP_H_
#define _RDMAP_H_

/**
 * @name Hash map
 *
 * Memory of key and value are allocated by the user but owned by the hash map
 * until elements are deleted or overwritten.
 *
 * @remark Not thread safe.
 */


typedef struct rd_map_s rd_map_t;

typedef struct rd_map_elem_s {
        LIST_ENTRY(rd_map_elem_s) hlink; /**< Hash bucket link */
        LIST_ENTRY(rd_map_elem_s) link;  /**< Iterator link */
        unsigned int hash;               /**< Key hash value */
        const void *key;                 /**< Key (memory owned by map) */
        const void *value;               /**< Value (memory owned by map) */
} rd_map_elem_t;



/**
 * @brief Set/overwrite value in map.
 *
 * If an existing entry with the same key already exists its key and value
 * will be freed with the destroy_key and destroy_value functions
 * passed to rd_map_init().
 *
 * The map assumes memory ownership of both the \p key and \p value and will
 * use the destroy_key and destroy_value functions (if set) to free
 * the key and value memory when the map is destroyed or element removed.
 */
void rd_map_set (rd_map_t *rmap, void *key, void *value);


/**
 * @brief Look up \p key in the map and return its value, or NULL
 *        if \p key was not found.
 *
 * The returned memory is still owned by the map.
 */
const void *rd_map_get (rd_map_t *rmap, const void *key);


/**
 * @brief Delete \p key from the map, if it exists.
 *
 * The destroy_key and destroy_value functions (if set) will be used
 * to free the key and value memory.
 */
void rd_map_delete (rd_map_t *rmap, const void *key);


/**
 * @returns the current number of elements in the map.
 */
size_t rd_map_cnt (rd_map_t *rmap);



/**
 * @brief Iterate over all elements in the map.
 *
 * @warning The map MUST NOT be mutated during the loop.
 */
#define RD_MAP_FOREACH(ELEM,RMAP)                  \
        for (rd_map_iter_begin((RMAP), &(ELEM)) ;  \
             rd_map_iter(&(ELEM)) ;                \
             rd_map_iter_next(&(ELEM)))

/**
 * @brief Begin iterating \p rmap, first element is set in \p *elem.
 */
void rd_map_iter_begin (const rd_map_t *rmap, rd_map_elem_t **elem);

/**
 * @returns 1 if \p *elem is a valid iteration element, else 0.
 */
static RD_INLINE RD_UNUSED
int rd_map_iter (rd_map_elem_t **elem) {
        return *elem != NULL;
}

/**
 * @brief Advances the iteration to the next element.
 */
static RD_INLINE RD_UNUSED
void rd_map_iter_next (rd_map_elem_t **elem) {
        *elem = LIST_NEXT(*elem, link);
}


/**
 * @brief Initialize a map that is expected to hold \p expected_cnt elements.
 *
 * @param expected_cnt Expected number of elements in the map,
 *                     this is used to select a suitable bucket count.
 *                     Passing a value of 0 will set the bucket count
 *                     to a reasonable default.
 * @param cmp Key comparator that must return 0 if the two keys match.
 * @param hash Key hashing function that is used to map a key to a bucket.
 *             It must return an integer hash >= 0 of the key.
 * @param destroy_key (Optional) When an element is deleted or overwritten
 *                    this function will be used to free the key memory.
 * @param destroy_value (Optional) When an element is deleted or overwritten
 *                      this function will be used to free the value memory.
 *
 * Destroy the map with rd_map_destroy()
 *
 * @remarks The map is not thread-safe.
 */
void rd_map_init (rd_map_t *rmap, size_t expected_cnt,
                  int (*cmp) (const void *a, const void *b),
                  unsigned int (*hash) (const void *key),
                  void (*destroy_key) (void *key),
                  void (*destroy_value) (void *value));


/**
 * @brief Free all elements in the map and free all memory associated
 *        with the map, but not the rd_map_t itself.
 *
 * The map is unusable after this call but can be re-initialized using
 * rd_map_init().
 */
void rd_map_destroy (rd_map_t *rmap);

#endif /* _RDMAP_H_ */
