/**\file stringpool.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2010-2011 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#ifndef LIBDENG_STRINGPOOL_H
#define LIBDENG_STRINGPOOL_H

#include "dd_string.h"

struct stringpool_intern_s;

// Intern string identifier.
typedef uint stringpool_internid_t;

/**
 * String Pool.  Simple data structure for managing a set of unique strings
 * with integral interning mechanism.
 *
 * @ingroup data
 *
 * @todo Performance is presently suboptimal due to the representation of the
 * intern list. A better implementation would use a binary search tree.
 */
typedef struct stringpool_s {
    /// Intern list (StringPool::_internsCount size).
    struct stringpool_intern_s* _interns;
    uint _internsCount;

    /// Sorted redirection table.
    stringpool_internid_t* _sortedInternTable;
} stringpool_t;

/**
 * @param strings  Array of strings to be interned (must contain at least @a count strings!)
 * @param count  Number of strings to be interned.
 */
stringpool_t* StringPool_Construct(ddstring_t** strings, uint count);
stringpool_t* StringPool_ConstructDefault(void);
void StringPool_Destruct(stringpool_t* pool);

/// Clear the string pool (reset to default initial state).
void StringPool_Clear(stringpool_t* pool);

/// @return  @c true if there are no strings present in the pool.
boolean StringPool_Empty(stringpool_t* pool);

/// @return  Number of strings in the pool.
uint StringPool_Size(stringpool_t* pool);

/**
 * Intern string @a str into the pool. If this is not a previously known string
 * a new intern is created otherwise the existing intern is re-used.
 *
 * \note If @a str is invalid this method does not return and a fatal error is thrown.
 *
 * @param str  String to be interned (must not be of zero-length).
 * @return  Unique Id associated with the interned copy of @a str.
 */
stringpool_internid_t StringPool_Intern(stringpool_t* pool, const ddstring_t* str);

/**
 * Same as StringPool::Intern execpt the interned copy of the string is returned
 * rather than the intern Id.
 */
const ddstring_t* StringPool_InternAndRetrieve(stringpool_t* pool, const ddstring_t* str);

/**
 * Have we already interned @a str?
 * @param str  Candidate string to look for.
 * @return  Id associated with the interned copy of @a str if found, else @c 0
 */
stringpool_internid_t StringPool_IsInterned(stringpool_t* pool, const ddstring_t* str);

/**
 * Retrieve an immutable copy of the interned string associated with @a internId.
 *
 * \note If @a internId is invalid this method does not return and a fatal error is thrown.
 *
 * @param internId  Id of the interned string to retrieve.
 * @return  Interned string associated with @a internId.
 */
const ddstring_t* StringPool_String(stringpool_t* pool, stringpool_internid_t internId);

#endif /* LIBDENG_STRINGPOOL_H */
