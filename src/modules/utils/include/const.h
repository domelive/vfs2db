/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file   const.h
 * @author Domenico Livera (domenico.livera@gmail.com)
 * @author Nicola Travaglini (nicola1.travaglini@gmail.com)
 * @brief  Constant definitions and utility macros.
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

#ifndef CONST_H
#define CONST_H

#include <stdint.h>

// =============================================================
// Utility Macros
// =============================================================

#define MIN(a, b) (((a) < (b)) ? (a) : (b)) /**< Macro to get the minimum of two values */

/**
 * @brief Macro to count occurrences of a character in a string.
 *
 * @param[in] str The input string.
 * @param[in] ch  The character to count.
 *
 * @return The number of occurrences of `ch` in `str`.
 */
#define COUNT_CHAR(str, ch)                                                                        \
    ({                                                                                             \
        int count = 0;                                                                             \
        for (int i = 0; str[i] != '\0'; i++) {                                                     \
            if (str[i] == ch)                                                                      \
                count++;                                                                           \
        }                                                                                          \
        count;                                                                                     \
    })

// =============================================================
// Constant Definitions
// =============================================================

#define MB(n)    (n * (1024 * 1024)) /**< Megabyte in bytes */
#define MAX_SIZE (16384 * 4)         /**< Maximum size for arrays and buffers */

#define CACHE_SIZE   MB(128)                   /**< Total cache size in bytes */
#define BLOCK_SIZE   (256 * 1024)              /**< Size of chunks for reading/writing operations */
#define CACHE_BLOCKS (CACHE_SIZE / BLOCK_SIZE) /**< Number of blocks in the cache */

#define ARENA_DEFAULT_SIZE MB(1) /**< Default size for memory arenas */

#endif // CONST_H