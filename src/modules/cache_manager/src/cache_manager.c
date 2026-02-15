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
 */
static struct {
    CacheBlock*     map;
    CacheBlock*     lru_head;
    CacheBlock*     lru_tail;
    pthread_mutex_t lock;

    unsigned long hits;
    unsigned long misses;
    unsigned long evictions;
} cache = {.map       = NULL,
           .lru_head  = NULL,
           .lru_tail  = NULL,
           .lock      = PTHREAD_MUTEX_INITIALIZER,
           .hits      = 0,
           .misses    = 0,
           .evictions = 0};

/**
 * @brief Retrieves a cache block from the cache based on the given key.
 *
 * This function looks up a cache block using the provided key. If the block
 * is found, it is touched to update its position in the LRU list.
 *
 * @param[in] key Pointer to the cache key to look for.
 *
 * @return Pointer to the found cache block, or NULL if not found.
 */
CacheBlock* cache_get(CacheKey* key) {
    CacheBlock* blk = NULL;

    pthread_mutex_lock(&cache.lock);

    HASH_FIND(hh, cache.map, key, sizeof(CacheKey), blk);
    if (blk) {
        cache_touch(blk);
        cache.hits++;
        LOG_TRACE("Cache HIT [%lu/%lu]: path='%s', offset=%ld", cache.hits,
                  cache.hits + cache.misses, key->query, key->offset);
    } else {
        cache.misses++;
        LOG_TRACE("Cache MISS [%lu/%lu]: path='%s', offset=%ld", cache.misses,
                  cache.hits + cache.misses, key->query, key->offset);
    }

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
 */
static inline void cache_insert_head(CacheBlock* blk) {
    blk->prev = NULL;
    blk->next = cache.lru_head;

    if (cache.lru_head)
        cache.lru_head->prev = blk;
    cache.lru_head = blk;

    if (cache.lru_tail == NULL)
        cache.lru_tail = blk;

    LOG_TRACE("Inserted block at head: path='%s', offset=%ld", blk->key.query, blk->key.offset);
}

/**
 * @brief Returns the current number of cache blocks in the cache.
 *
 * @return The number of cache blocks currently stored in the cache.
 *
 */
int cache_count() { return HASH_COUNT(cache.map); }

/**
 * @brief Inserts a cache block into the cache and updates the LRU list.
 *
 * @param[in] blk Pointer to the cache block to insert.
 *
 * This function adds the specified cache block to the hash map and
 * places it at the head of the LRU list, indicating that it was
 * recently accessed.
 */
void cache_add_block(CacheBlock* blk) {
    // P2 deve aspettare che P1 esca da cache_get
    pthread_mutex_lock(&cache.lock);

    CacheBlock* existing_blk = NULL;
    HASH_FIND(hh, cache.map, &blk->key, sizeof(CacheKey), existing_blk);

    // P1 in cache_get
    if (existing_blk) {
        LOG_DEBUG("Block already exists in cache, skipping: path='%s', offset=%ld", blk->key.query,
                  blk->key.offset);

        // FIX: we should delete it
        pthread_mutex_unlock(&cache.lock);
        return;
    }

    // Evict if cache is full
    int current_count = cache_count();
    if (current_count >= CACHE_BLOCKS) {
        LOG_DEBUG("Cache full (%d/%d blocks), evicting least recently used block", current_count,
                  CACHE_BLOCKS);
        cache_evict();
        cache.evictions++;
    }

    // Add to hash map
    HASH_ADD(hh, cache.map, key, sizeof(CacheKey), blk);
    cache_insert_head(blk);

    LOG_DEBUG("Added block to cache [%d/%d]: path='%s', offset=%ld, size=%zu", cache_count(),
              CACHE_BLOCKS, blk->key.query, blk->key.offset, blk->actual_size);

    pthread_mutex_unlock(&cache.lock);
}

/**
 * @brief Touches a cache block to update its position in the LRU list.
 *
 * @param[in] blk Pointer to the cache block to touch.
 *
 * This function moves the specified cache block to the head of the LRU list,
 * indicating that it was recently accessed.
 */
void cache_touch(CacheBlock* blk) {
    // If already at head, nothing to do
    if (blk == cache.lru_head)
        return;

    LOG_TRACE("Touching block (moving to head): path='%s', offset=%ld", blk->key.query,
              blk->key.offset);

    // Remove from current position
    if (blk->next)
        blk->next->prev = blk->prev;
    if (blk->prev)
        blk->prev->next = blk->next;
    if (blk == cache.lru_tail)
        cache.lru_tail = blk->prev;

    cache_insert_head(blk);
}

/**
 * @brief Evicts the least recently used cache block from the cache.
 *
 * This function removes the cache block at the tail of the LRU list,
 * effectively evicting the least recently used block from the cache.
 */
void cache_evict() {
    if (!cache.lru_tail) {
        LOG_WARN("Cache eviction called but cache is empty");
        return;
    }

    CacheBlock* to_evict = cache.lru_tail;

    LOG_DEBUG("Evicting block: path='%s', offset=%ld", to_evict->key.query, to_evict->key.offset);

    // Remove from hash map
    HASH_DEL(cache.map, to_evict);

    // Update LRU pointers
    if (to_evict->prev) {
        to_evict->prev->next = NULL;
        cache.lru_tail       = to_evict->prev;
    } else {
        cache.lru_head = NULL;
        cache.lru_tail = NULL;
    }

    // Free the evicted block
    if (to_evict->data)
        free(to_evict->data);
    free(to_evict);

    LOG_TRACE("Eviction complete. Total evictions: %lu", cache.evictions);
}

/**
 * @brief Evicts a specific cache block from the cache based on the given key.
 *
 * @param[in] key Pointer to the cache key of the block to evict.
 */
void cache_evict_block(CacheKey* key) {
    pthread_mutex_lock(&cache.lock);

    CacheBlock* blk = NULL;
    LOG_TRACE("Looking for block to evict: path='%s', offset=%ld", key->query, key->offset);
    HASH_FIND(hh, cache.map, key, sizeof(CacheKey), blk);
    LOG_TRACE("Block lookup complete for eviction");
    if (!blk) {
        LOG_TRACE("No block found to evict for key: path='%s', offset=%ld", key->query,
                  key->offset);
        pthread_mutex_unlock(&cache.lock);
        return;
    }

    LOG_DEBUG("Evicting specific block: path='%s', offset=%ld", blk->key.query, blk->key.offset);

    cache.evictions++;

    // Remove from hash map
    HASH_DEL(cache.map, blk);

    // Update LRU pointers
    if (blk->prev)
        blk->prev->next = blk->next;
    if (blk->next)
        blk->next->prev = blk->prev;
    if (blk == cache.lru_head)
        cache.lru_head = blk->next;
    if (blk == cache.lru_tail)
        cache.lru_tail = blk->prev;

    LOG_TRACE("Before free");
    // Free the evicted block
    if (blk->data)
        free(blk->data);
    free(blk);
    LOG_TRACE("After free");

    pthread_mutex_unlock(&cache.lock);
}

/**
 * @brief Prints the current state of the cache for debugging purposes.
 *
 * This function displays the number of blocks in the cache, the hit/miss/eviction
 * statistics, and the order of blocks in the LRU list if the log level is set to TRACE.
 */
void cache_view() {
    pthread_mutex_lock(&cache.lock);

    int count = cache_count();
    LOG_DEBUG("=== Cache View ===");
    LOG_DEBUG("Blocks: %d/%d (%.1f%% full)", count, CACHE_BLOCKS,
              (float)count / CACHE_BLOCKS * 100);
    LOG_DEBUG("Stats: hits=%lu, misses=%lu, evictions=%lu, hit_rate=%.1f%%", cache.hits,
              cache.misses, cache.evictions,
              cache.hits + cache.misses > 0 ? (float)cache.hits / (cache.hits + cache.misses) * 100
                                            : 0.0);

    if (logger_get_level() <= LOG_LEVEL_TRACE) {
        LOG_TRACE("LRU order (most recent first):");
        CacheBlock* current = cache.lru_head;
        int         idx     = 0;
        while (current) {
            LOG_TRACE("  [%d] query='%s', offset=%ld, size=%zu", idx++, current->key.query,
                      current->key.offset, current->actual_size);
            current = current->next;
        }
    }

    LOG_DEBUG("==================");

    pthread_mutex_unlock(&cache.lock);
}

/**
 * @brief Retrieves cache statistics.
 *
 * @param[out] hits Pointer to store the number of cache hits.
 * @param[out] misses Pointer to store the number of cache misses.
 * @param[out] evictions Pointer to store the number of cache evictions.
 */
void cache_get_stats(unsigned long* hits, unsigned long* misses, unsigned long* evictions) {
    pthread_mutex_lock(&cache.lock);
    if (hits)
        *hits = cache.hits;
    if (misses)
        *misses = cache.misses;
    if (evictions)
        *evictions = cache.evictions;
    pthread_mutex_unlock(&cache.lock);
}