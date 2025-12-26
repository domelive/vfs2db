/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file   db_handler.c
 * @author Domenico Livera (domenico.livera@gmail.com)
 * @author Nicola Travaglini (nicola1.travaglini@gmail.com)
 * @brief  Database Handler Source File
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

#ifndef ERRORS_H
#define ERRORS_H

/**
 * Status Codes
 * 
 * @brief Enumeration of status codes used throughout the database handler.
 * 
 */
typedef enum status_t {
    STATUS_OK,
    STATUS_DB_ERROR,
} status_t;

#endif // ERRORS_H