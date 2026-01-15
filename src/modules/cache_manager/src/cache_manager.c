/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file   db_handler.h
 * @author Domenico Livera (domenico.livera@gmail.com)
 * @author Nicola Travaglini (nicola1.travaglini@gmail.com)
 * @brief  Database Handler Header File
 * @date   Created on 2025-12-23
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "cache_manager.h"

/**
 * Cache Structure
 * 
 * @brief Structure representing the cache state, including
 *        the hash map of cache blocks and the LRU list pointers.
 * 
 */
static struct {
    CacheBlock* map;
    CacheBlock* lru_head;
    CacheBlock* lru_tail;
    pthread_mutex_t lock;
} cache = {NULL, NULL, NULL, PTHREAD_MUTEX_INITIALIZER};

/**
 * @brief Retrieves a cache block from the cache based on the given key.
 * 
 * This function looks up a cache block using the provided key. If the block
 * is found, it is touched to update its position in the LRU list.
 * 
 * @param[in] key Pointer to the cache key to look for.
 * 
 * @return Pointer to the found cache block, or NULL if not found.
 * 
 */
CacheBlock* cache_get(CacheKey* key) {
    CacheBlock* blk = NULL;
    
    pthread_mutex_lock(&cache.lock);

    HASH_FIND(hh, cache.map, key, sizeof(CacheKey), blk);
    if (blk) cache_touch(blk);

    pthread_mutex_unlock(&cache.lock);

    return blk;
}

/**
 * @brief Inserts a cache block at the head of the LRU list.
 * 
 * @param[in] blk Pointer to the cache block to insert.
 * 
 * This function places the specified cache block at the head of the LRU list,
 * indicating that it was recently accessed.
 * 
 */
static inline void cache_insert_head(CacheBlock* blk) {
    blk->prev = NULL;
    blk->next = cache.lru_head;

    if (cache.lru_head) cache.lru_head->prev = blk;
    cache.lru_head = blk;

    if (cache.lru_tail == NULL) cache.lru_tail = blk;
}

/**
 * @brief Returns the current number of cache blocks in the cache.
 * 
 * @return The number of cache blocks currently stored in the cache.
 * 
 */
int cache_count() {
    return HASH_COUNT(cache.map);
}

/**
 * @brief Inserts a cache block into the cache and updates the LRU list.
 * 
 * @param[in] blk Pointer to the cache block to insert.
 * 
 * This function adds the specified cache block to the hash map and
 * places it at the head of the LRU list, indicating that it was
 * recently accessed.
 * 
 */
void cache_add_block(CacheBlock* blk) {
    // P2 deve aspettare che P1 esca da cache_get
    pthread_mutex_lock(&cache.lock);

    CacheBlock* existing_blk = NULL;
    HASH_FIND(hh, cache.map, &blk->key, sizeof(CacheKey), existing_blk);
    
    // P1 in cache_get
    if (existing_blk) {
        // FIX: we should delete it
        pthread_mutex_unlock(&cache.lock);
        return;
    }

    // Evict if cache is full
    if (cache_count() >= CACHE_BLOCKS) {
        printf("\n --- MI PIACE LA BEGA (E ANCHE L'EVICT) --- \n");
        cache_evict();
    }
    
    // Add to hash map
    HASH_ADD(hh, cache.map, key, sizeof(CacheKey), blk);
    cache_insert_head(blk);

    pthread_mutex_unlock(&cache.lock);
}

/**
 * @brief Touches a cache block to update its position in the LRU list.
 * 
 * @param[in] blk Pointer to the cache block to touch.
 * 
 * This function moves the specified cache block to the head of the LRU list,
 * indicating that it was recently accessed.
 * 
 */
void cache_touch(CacheBlock* blk) {
    // If already at head, nothing to do
    if (blk == cache.lru_head) return;

    // Remove from current position
    if (blk->next) blk->next->prev = blk->prev;
    if (blk->prev) blk->prev->next = blk->next;
    if (blk == cache.lru_tail) cache.lru_tail = blk->prev;

    cache_insert_head(blk);
}

/**
 * @brief Evicts the least recently used cache block from the cache.
 * 
 * This function removes the cache block at the tail of the LRU list,
 * effectively evicting the least recently used block from the cache.
 * 
 */
void cache_evict() {
    if (cache.lru_tail) {
        CacheBlock* to_evict = cache.lru_tail;

        // Remove from hash map
        HASH_DEL(cache.map, to_evict);

        // Update LRU pointers
        if (to_evict->prev) {
            to_evict->prev->next = NULL;
            cache.lru_tail = to_evict->prev;
        } else {
            cache.lru_head = NULL;
            cache.lru_tail = NULL;
        }

        // Free the evicted block
        free(to_evict);
    }
}

void cache_view() {
    CacheBlock* current = cache.lru_head;
    printf("Cache Blocks (Most Recent to Least Recent): %d\n", cache_count());
    while (current) {
        printf("Key: [%s, %ld]\n", current->key.query, current->key.offset);
        current = current->next;
    }
}