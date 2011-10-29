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

#ifdef __cplusplus
extern "C" {
#endif

#include "dd_types.h"

/**
 * String Pool.  Simple data structure for managing a set of unique case-insensitive
 * strings with integral interning mechanism.
 *
 * @ingroup data
 */
struct stringpool_s; // The stringpool instance (opaque).
typedef struct stringpool_s StringPool;

// Interned string identifier.
typedef uint StringPoolInternId;

/**
 * @param strings  Array of strings to be interned (must contain at least @a count strings!)
 * @param count  Number of strings to be interned.
 */
StringPool* StringPool_New(void);
StringPool* StringPool_NewWithStrings(const ddstring_t* strings, uint count);
void StringPool_Delete(StringPool* pool);

/// Clear the string pool (reset to default initial state).
void StringPool_Clear(StringPool* pool);

/// @return  @c true if there are no strings present in the pool.
boolean StringPool_Empty(StringPool* pool);

/// @return  Number of strings in the pool.
uint StringPool_Size(StringPool* pool);

/**
 * Intern string @a str into the pool. If this is not a previously known string
 * a new intern is created otherwise the existing intern is re-used.
 *
 * \note If @a str is invalid this method does not return and a fatal error is thrown.
 *
 * @param str  String to be interned (must not be of zero-length).
 * @return  Unique Id associated with the interned copy of @a str.
 */
StringPoolInternId StringPool_Intern(StringPool* pool, const ddstring_t* str);

/**
 * Same as StringPool::Intern execpt the interned copy of the string is returned
 * rather than the intern Id.
 */
const ddstring_t* StringPool_InternAndRetrieve(StringPool* pool, const ddstring_t* str);

/**
 * Have we already interned @a str?
 *
 * @param str  Candidate string to look for.
 * @return  Id associated with the interned copy of @a str if found, else @c 0
 */
StringPoolInternId StringPool_IsInterned(StringPool* pool, const ddstring_t* str);

/**
 * Retrieve an immutable copy of the interned string associated with @a internId.
 *
 * @param internId  Id of the interned string to retrieve.
 * @return  Interned string associated with @a internId.
 */
const ddstring_t* StringPool_String(StringPool* pool, StringPoolInternId internId);

/**
 * Remove an interned string from the pool.
 *
 * @param str  String to be removed.
 * @return  @c true if string @a str was present and subsequently removed.
 */
boolean StringPool_Remove(StringPool* pool, ddstring_t* str);

/// Same as StringPool::Remove except the argument is StringPoolInternId.
boolean StringPool_RemoveIntern(StringPool* pool, StringPoolInternId internId);

#if _DEBUG
/**
 * Print contents of the pool. For debug.
 */
void StringPool_Print(StringPool* pool);
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_STRINGPOOL_H */
