/**
 * @file stringpool.h
 * String pool (case insensitive). @ingroup base
 *
 * Data structure for storing a set of unique case-insensitive strings. When
 * multiple strings with the same case-insensitive contents are added to the
 * pool, only one copy of the string is kept in memory. If there is variation
 * in the letter cases, the version added first is used. In other words, the
 * letter cases of the later duplicate strings are ignored.
 *
 * Each logical copy of a string added to the pool gets its own unique
 * identifier, even though they may be sharing the same internal copy of the
 * string.
 *
 * @todo Add case-sensitive mode.
 *
 * @authors Copyright © 2010-2012 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBDENG_STRINGPOOL_H
#define LIBDENG_STRINGPOOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "dd_types.h"

struct stringpool_s; // The stringpool instance (opaque).

/**
 * StringPool instance. Use StringPool_New() or one of the other constructs to create.
 */
typedef struct stringpool_s StringPool;

/// String identifier. Each logical string instance has its own Id.
typedef uint StringPoolId;

/**
 * Constructs an empty StringPool. The pool must be destroyed with
 * StringPool_Delete() when no longer needed.
 */
StringPool* StringPool_New(void);

/**
 * Constructs a new string pool and adds a number of strings into it. The pool
 * must be destroyed with StringPool_Delete() when no longer needed.
 *
 * @param strings  Array of strings to be interned (must contain at least @a count strings!)
 * @param count  Number of strings to be interned.
 */
StringPool* StringPool_NewWithStrings(const ddstring_t* strings, uint count);

/**
 * Destroys the stringpool.
 * @param pool  StringPool instance.
 */
void StringPool_Delete(StringPool* pool);

/**
 * Clear the string pool (reset to default initial state).
 * @param pool  StringPool instance.
 */
void StringPool_Clear(StringPool* pool);

/**
 * @param pool  StringPool instance.
 * @return  @c true if there are no strings present in the pool.
 */
boolean StringPool_Empty(const StringPool* pool);

/**
 * @param pool  StringPool instance.
 * @return Number of strings in the pool. The actual number of strings stored
 * in memory may be less than this.
 */
uint StringPool_Size(const StringPool* pool);

/**
 * Adds string @a str into the pool. One logical copy of the string is added
 * to the pool. If this is not a previously known string, a new internal copy
 * is created; otherwise, the existing interned copy is re-used.
 *
 * @note If @a str is invalid this method does not return and a fatal error is
 * thrown.
 *
 * @param pool  StringPool instance.
 * @param str  String to be added (must not be of zero-length).
 *
 * @return  Unique Id associated with the logical copy of @a str.
 */
StringPoolId StringPool_Add(StringPool* pool, const ddstring_t* str);

/**
 * Adds string @a str into the pool. One logical copy of the string is added
 * to the pool. If this is not a previously known string, a new internal copy
 * is created; otherwise, the existing interned copy is re-used.
 *
 * @param pool  StringPool instance.
 * @param str  String to be added (must not be of zero-length).
 *
 * @return The interned copy of the string owned by the pool.
 * (Not the same as @a str.)
 */
const ddstring_t* StringPool_AddAndRetrieve(StringPool* pool, const ddstring_t* str);

/**
 * Sets the user-specified custom value associated with the string @a id.
 */
void StringPool_SetUserValue(StringPool* pool, StringPoolId id, uint value);

/**
 *
 */
uint StringPool_UserValue(StringPool* pool, StringPoolId id);

/**
 * Is there at least one copy of @a str in the pool?
 *
 * @param pool  StringPool instance.
 * @param str  Candidate string to look for.
 *
 * @return  Id of the first matching string; else @c 0
 */
StringPoolId StringPool_IsInterned(const StringPool* pool, const ddstring_t* str);

/**
 * Retrieve an immutable copy of the interned string associated with logical
 * string @a id.
 *
 * @param pool  StringPool instance.
 * @param id  Id of the string to retrieve.
 *
 * @return  Interned string associated with @a internId.
 */
const ddstring_t* StringPool_String(const StringPool* pool, StringPoolId id);

/**
 * Remove one logical instance of a string from the pool. This is the inverse
 * of StringPool_Add(). If you have added a string @em n times, you will need
 * to call StringPool_Remove() @em n times before the string is really freed
 * from memory.
 *
 * @param pool  StringPool instance.
 * @param str  String to be removed.
 *
 * @return  @c true if string @a str was present and was removed.
 */
boolean StringPool_RemoveOne(StringPool* pool, const ddstring_t* str);

/**
 * Remove one logical instance of a string from the pool. This is the inverse
 * of StringPool_Add(). When all instances of the a string are gone, the copy
 * of the string owned by the pool is freed from memory.
 *
 * @param pool  StringPool instance.
 * @param id  Id of the string to remove.
 *
 * @return  @c true if the string was removed.
 */
boolean StringPool_RemoveAllById(StringPool* pool, StringPoolId id);

/**
 * Iterate over all strings in the pool making a callback for each. Iteration
 * ends when all strings have been processed or a callback returns non-zero.
 *
 * @param pool  StringPool instance.
 * @param callback  Callback to make for each iteration.
 * @param parameters  User data to be passed to the callback.
 *
 * @return  @c 0 iff iteration completed wholly.
 */
int StringPool_IterateAll(const StringPool* pool, int (*callback)(StringPoolId, void*), void* data);

/**
 * Serializes the pool using @a writer.
 * @param ar StringPool instance.
 * @param writer  Writer instance.
 */
void StringPool_Write(const StringPool* ar, Writer* writer);

/**
 * Deserializes the pool from @a reader.
 * @param ar StringPool instance.
 * @param reader  Reader instance.
 */
void StringPool_Read(StringPool* ar, Reader* reader);

#if _DEBUG
/**
 * Print contents of the pool. For debug.
 * @param pool  StringPool instance.
 */
void StringPool_Print(const StringPool* pool);
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_STRINGPOOL_H */
