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

#ifndef CACHE_MANAGER_H
#define CACHE_MANAGER_H

#include <pthread.h>
#include <stdio.h>
#include <sys/types.h>

#include "const.h"
#include "errors.h"
#include "logger.h"
#include "uthash.h"

/**
 * @brief Structure representing a key for cache blocks.
 *
 * Includes the following fields:
 * - `query`:   The database query string associated with the cache block.
 * - `offset`:  The offset within the query result that this cache block represents.
 */
typedef struct CacheKey {
    char  query[MAX_SIZE];
    off_t offset;
} CacheKey;

/**
 * @brief Structure representing a cache block for storing query results.
 *
 * Includes the following fields:
 * - `key`:          The `CacheKey` associated with this cache block.
 * - `data`:        Pointer to the data stored in this cache block.
 * - `actual_size`: The actual size of the data stored in this cache block.
 * - `prev`:        Pointer to the previous cache block in the linked list.
 * - `next`:        Pointer to the next cache block in the linked list.
 * - `hh`:          UTHash handle for hash table operations.
 */
typedef struct CacheBlock {
    CacheKey key;

    void*  data;
    size_t actual_size;

    struct CacheBlock* prev;
    struct CacheBlock* next;

    UT_hash_handle hh;
} CacheBlock;

/**
 * @brief Inserts a cache block into the cache and updates the LRU list.
 *
 * @param[in] blk Pointer to the cache block to insert.
 *
 * This function adds the specified cache block to the hash map and
 * places it at the head of the LRU list, indicating that it was
 * recently accessed.
 */
void cache_add_block(CacheBlock* blk);

/**
 * @brief Touches a cache block to update its position in the LRU list.
 *
 * This function moves the specified cache block to the head of the LRU list,
 * indicating that it was recently accessed.
 *
 * @param[in] blk Pointer to the cache block to touch.
 */
void cache_touch(CacheBlock* blk);

/**
 * @brief Evicts the least recently used cache block from the cache.
 *
 * This function removes the cache block at the tail of the LRU list,
 * effectively evicting the least recently used block from the cache.
 */
void cache_evict();

/**
 * @brief Evicts a specific cache block from the cache based on the given key.
 *
 * @param[in] key Pointer to the cache key of the block to evict.
 */
void cache_evict_block(CacheKey* key);

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
CacheBlock* cache_get(CacheKey* key);

/**
 * @brief Prints the current state of the cache for debugging purposes.
 *
 * This function displays the number of blocks in the cache, the hit/miss/eviction
 * statistics, and the order of blocks in the LRU list if the log level is set to TRACE.
 */
void cache_view();

/**
 * @brief Retrieves cache statistics.
 *
 * @param[out] hits Pointer to store the number of cache hits.
 * @param[out] misses Pointer to store the number of cache misses.
 * @param[out] evictions Pointer to store the number of cache evictions.
 */
void cache_get_stats(unsigned long* hits, unsigned long* misses, unsigned long* evictions);

#endif // CACHE_MANAGER_H