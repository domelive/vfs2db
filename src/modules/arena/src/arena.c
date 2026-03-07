/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file   arena.c
 * @author Domenico Livera (domenico.livera@gmail.com)
 * @author Nicola Travaglini (nicola1.travaglini@gmail.com)
 * @brief  Arena Source File
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

#include "arena.h"

Arena* arena_create(size_t size) {
    uint8_t* mem = malloc(sizeof(Arena) + size);
    if (!mem) {
        LOG_ERROR("Failed to allocate memory for arena of size %zu", size);
        return NULL;
    }

    Arena* arena = (Arena*)mem;

    arena->base_pos   = mem + sizeof(Arena);
    arena->total_size = size;
    arena->offset     = 0;

    return arena;
}

void arena_destroy(Arena* arena) {
    assert(arena != NULL && "arena_destroy called with NULL arena");

    free(arena);
}

void* arena_alloc(Arena* arena, size_t size) {
    assert(arena != NULL && "arena_alloc called with NULL arrena");

    // Calculate the padding needed to align the allocation to ARENA_ALIGNMENT bytes. This ensures
    // that all allocations from the arena are properly aligned, which can improve performance and
    // prevent issues on architectures that require aligned access.
    uintptr_t padding = (ARENA_ALIGNMENT - (arena->offset % ARENA_ALIGNMENT)) % ARENA_ALIGNMENT;

    // Check if there is enough space in the arena to accommodate the requested size along with the
    // necessary padding for alignment.
    if (arena->offset + padding + size > arena->total_size) {
        LOG_ERROR("Arena out of memory: requested %zu bytes, available %zu bytes", size,
                  arena_get_available_size(arena));
        return NULL;
    }

    // Update the arena's offset to account for the padding and allocate the requested memory block.
    arena->offset += padding;

    // Calculate the pointer to the allocated memory block based on the base position and the
    // current offset, and return it to the caller.
    void* ptr = arena->base_pos + arena->offset;

    // Update the arena's offset to reflect the allocated memory block, so that the next allocation
    // will start after the current block.
    arena->offset += size;

    return ptr;
}

void* arena_calloc(Arena* arena, size_t count, size_t size) {
    assert(arena != NULL && "arena_calloc called with NULL arena");

    void* ptr = arena_alloc(arena, count * size);

    if (ptr) {
        LOG_TRACE("arena_calloc: zero-initializing %zu bytes", count * size);
        memset(ptr, 0, count * size);
    }

    return ptr;
}

char* arena_strdup(Arena* arena, const char* str) {
    assert(arena != NULL && "arena_strdup called with NULL arena");
    assert(str != NULL && "arena_strdup called with NULL string");

    size_t len     = strlen(str) + 1; // +1 for null terminator
    char*  dup_str = (char*)arena_alloc(arena, len);

    if (dup_str) {
        LOG_TRACE("arena_strdup: duplicating string of length %zu", len - 1);
        memcpy(dup_str, str, len);
        dup_str[len - 1] = '\0'; // Ensure null termination
    }

    return dup_str;
}

void arena_reset(Arena* arena) {
    assert(arena != NULL && "arena_reset called with NULL arena");
    arena->offset = 0;
}

size_t arena_get_used_size(Arena* arena) {
    assert(arena != NULL && "arena_get_used_size called with NULL arena");
    return arena->offset;
}

size_t arena_get_available_size(Arena* arena) {
    assert(arena != NULL && "arena_get_available_size called with NULL arena");
    return arena->total_size - arena->offset;
}

size_t arena_get_total_size(Arena* arena) {
    assert(arena != NULL && "arena_get_total_size called with NULL arena");
    return arena->total_size;
}

void arena_view(Arena* arena) {
    assert(arena != NULL && "arena_view called with NULL arena");

    size_t used      = arena_get_used_size(arena);
    size_t available = arena_get_available_size(arena);
    size_t total     = arena_get_total_size(arena);

    LOG_DEBUG("Arena View: Used: %zu bytes, Available: %zu bytes, Total: %zu bytes", used,
              available, total);
}