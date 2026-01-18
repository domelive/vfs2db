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

// =============================================================
// Initialization and Destruction Functions
// =============================================================

/**
 * @brief Creates a new memory arena of the specified size.
 * 
 * This function allocates a contiguous block of memory of the specified size
 * and initializes an `Arena` structure to manage it.
 * 
 * @param[in] size The total size of the arena to create.
 * 
 * @return Pointer to the newly created `Arena` structure, or NULL on failure.
 */
Arena* arena_create(size_t size);

/**
 * @brief Destroys the memory arena and frees all associated memory.
 * 
 * This function frees the memory block managed by the arena and
 * deallocates the `Arena` structure itself.
 * 
 * @param[in] arena Pointer to the `Arena` to destroy.
 */
void arena_destroy(Arena* arena);

// =============================================================
// Memory Allocation Functions
// =============================================================

/**
 * @brief Allocates memory from the arena.
 * 
 * This function allocates a block of memory of the specified size from the arena.
 * The allocated memory is aligned to `ARENA_ALIGNMENT` bytes.
 * 
 * @param[in] arena Pointer to the `Arena` from which to allocate memory.
 * @param[in] size  The size of the memory block to allocate.
 * 
 * @return Pointer to the allocated memory block, or NULL if there is insufficient space.
 */
void* arena_alloc(Arena* arena, size_t size);

/**
 * @brief Allocates zero-initialized memory from the arena.
 * 
 * This function allocates a block of memory for an array of `count` elements,
 * each of size `size`, from the arena. The allocated memory is initialized to zero
 * and aligned to `ARENA_ALIGNMENT` bytes.
 * 
 * @param[in] arena Pointer to the `Arena` from which to allocate memory.
 * @param[in] count Number of elements to allocate.
 * @param[in] size  Size of each element.
 * 
 * @return Pointer to the allocated memory block, or NULL if there is insufficient space.
 */
void* arena_calloc(Arena* arena, size_t count, size_t size);

/**
 * @brief Duplicates a string into the arena.
 * 
 * This function allocates memory for a copy of the specified string
 * from the arena and copies the string into the allocated memory.
 * 
 * @param[in] arena Pointer to the `Arena` from which to allocate memory.
 * @param[in] str   The string to duplicate.
 * 
 * @return Pointer to the duplicated string in the arena, or NULL if there is insufficient space.
 */
char* arena_strdup(Arena* arena, const char* str);

/**
 * @brief Resets the arena for reuse.
 * 
 * This function resets the arena's offset to zero, effectively
 * marking all previously allocated memory as free. Note that this
 * does not actually free any memory; it simply allows for reuse
 * of the arena's memory block.
 * 
 * @param[in] arena Pointer to the `Arena` to reset.
 */
void arena_reset(Arena* arena);

// =============================================================
// Arena Getter Functions
// =============================================================

/**
 * @brief Gets the used size of the arena.
 * 
 * @param[in] arena Pointer to the `Arena`.
 * 
 * @return Used size of the arena in bytes.
 */
size_t arena_get_used_size(Arena* arena);

/**
 * @brief Gets the available size of the arena.
 * 
 * @param[in] arena Pointer to the `Arena`.
 * 
 * @return Available size of the arena in bytes.
 */
size_t arena_get_available_size(Arena* arena);

/**
 * @brief Gets the total size of the arena.
 * 
 * @param[in] arena Pointer to the `Arena`.
 * 
 * @return Total size of the arena in bytes.
 */
size_t arena_get_total_size(Arena* arena);