/**
 * @file de/stringpool.h
 * String pool (case insensitive). @ingroup base
 *
 * Container data structure for a set of unique case-insensitive strings.
 * Comparable to a @c std::set with unique IDs assigned to each contained string.
 *
 * The term "intern" is used here to refer to the act of inserting a string to
 * the pool. As a result of interning a string, a new internal copy of the
 * string may be created in the pool.
 *
 * Each string that actually gets added to the pool is assigned a unique
 * identifier. If one tries to intern a string that already exists in the pool
 * (case insensitively speaking), no new internal copy is created and no new
 * identifier is assigned. Instead, the existing id of the previously interned
 * string is returned. The string identifiers are not unique over the lifetime
 * of the container: if a string is removed from the pool, its id is free to be
 * reassigned to the next new string. Zero is not a valid id.
 *
 * Each string can also have an associated, custom user-defined uint32 value.
 *
 * The implementation has, at worst, O(log n) complexity for addition, removal,
 * string lookup, and user value set/get.
 *
 * @par Build Options
 * Define the macro @c DENG_STRINGPOOL_ZONE_ALLOCS to make StringPool allocate
 * memory from the memory zone instead of with system malloc().
 *
 * @todo Add case-sensitive mode.
 *
 * @authors Copyright &copy; 2010-2012 Daniel Swanson <danij@dengine.net>
 * @authors Copyright &copy; 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

//#define DENG_STRINGPOOL_ZONE_ALLOCS

#include "libdeng.h"
#include "types.h"
#include "de/ddstring.h"

#ifdef __cplusplus
extern "C" {
#endif

struct stringpool_s; // The stringpool instance (opaque).

/**
 * StringPool instance. Use StringPool_New() or one of the other constructors to create.
 */
typedef struct stringpool_s StringPool;

/// String identifier. Each string is assigned its own Id.
typedef uint StringPoolId;

/**
 * Constructs an empty StringPool. The pool must be destroyed with
 * StringPool_Delete() when no longer needed.
 */
DENG_PUBLIC StringPool* StringPool_New(void);

/**
 * Constructs an empty StringPool and interns a number of strings. The pool
 * must be destroyed with StringPool_Delete() when no longer needed.
 *
 * @param strings  Array of strings to be interned (must contain at least @a count strings).
 * @param count  Number of strings to be interned.
 */
DENG_PUBLIC StringPool* StringPool_NewWithStrings(const ddstring_t* strings, uint count);

/**
 * Destroys the stringpool.
 * @param pool  StringPool instance.
 */
DENG_PUBLIC void StringPool_Delete(StringPool* pool);

/**
 * Clear the string pool. All strings in the pool will be destroyed.
 * @param pool  StringPool instance.
 */
DENG_PUBLIC void StringPool_Clear(StringPool* pool);

/**
 * Is the pool empty?
 * @param pool  StringPool instance.
 * @return  @c true if there are no strings present in the pool.
 */
DENG_PUBLIC boolean StringPool_Empty(const StringPool* pool);

/**
 * Determines the number of strings in the pool.
 * @param pool  StringPool instance.
 * @return Number of strings in the pool.
 */
DENG_PUBLIC uint StringPool_Size(const StringPool* pool);

/**
 * Interns string @a str. If this string is not already in the pool, a new
 * internal copy is created; otherwise, the existing internal copy is returned.
 * New internal copies will be assigned new identifiers.
 *
 * @param pool  StringPool instance.
 * @param str  String to be added (must not be of zero-length).
 *             A copy of this is made if the string is interned.
 *
 * @return  Unique Id associated with the internal copy of @a str.
 */
DENG_PUBLIC StringPoolId StringPool_Intern(StringPool* pool, const ddstring_t* str);

/**
 * Interns string @a str. If this string is not already in the pool, a new
 * internal copy is created; otherwise, the existing internal copy is returned.
 * New internal copies will be assigned new identifiers.
 *
 * @param pool  StringPool instance.
 * @param str  String to be added (must not be of zero-length).
 *             A copy of this is made if the string is interned.
 *
 * @return The interned copy of the string owned by the pool.
 */
DENG_PUBLIC const ddstring_t* StringPool_InternAndRetrieve(StringPool* pool, const ddstring_t* str);

/**
 * Sets the user-specified custom value associated with the string @a id.
 * The default user value is zero.
 *
 * @param pool   StringPool instance.
 * @param id     Id of a string.
 * @param value  User value.
 */
DENG_PUBLIC void StringPool_SetUserValue(StringPool* pool, StringPoolId id, uint value);

/**
 * Retrieves the user-specified custom value associated with the string @a id.
 * The default user value is zero.
 *
 * @param pool   StringPool instance.
 * @param id     Id of a string.
 *
 * @return User value.
 */
DENG_PUBLIC uint StringPool_UserValue(StringPool* pool, StringPoolId id);

/**
 * Sets the user-specified custom pointer associated with the string @a id.
 * By default the pointer is NULL.
 *
 * @note  User pointer values are @em not serialized.
 *
 * @param pool   StringPool instance.
 * @param id     Id of a string.
 * @param ptr    User pointer.
 */
DENG_PUBLIC void StringPool_SetUserPointer(StringPool* pool, StringPoolId id, void* ptr);

/**
 * Retrieves the user-specified custom pointer associated with the string @a id.
 *
 * @param pool   StringPool instance.
 * @param id     Id of a string.
 *
 * @return User pointer.
 */
DENG_PUBLIC void* StringPool_UserPointer(StringPool* pool, StringPoolId id);

/**
 * Is @a str considered to be in the pool?
 *
 * @param pool  StringPool instance.
 * @param str   String to look for.
 *
 * @return  Id of the matching string; else @c 0.
 */
DENG_PUBLIC StringPoolId StringPool_IsInterned(const StringPool* pool, const ddstring_t* str);

/**
 * Retrieve an immutable copy of the interned string associated with the
 * string @a id.
 *
 * @param pool  StringPool instance.
 * @param id    Id of the string to retrieve.
 *
 * @return  Interned string associated with @a internId. Owned by the pool.
 */
DENG_PUBLIC const ddstring_t* StringPool_String(const StringPool* pool, StringPoolId id);

/**
 * Removes a string from the pool.
 *
 * @param pool  StringPool instance.
 * @param str   String to be removed.
 *
 * @return  @c true, if string @a str was found and removed.
 */
DENG_PUBLIC boolean StringPool_Remove(StringPool* pool, const ddstring_t* str);

/**
 * Removes a string from the pool.
 *
 * @param pool  StringPool instance.
 * @param id    Id of the string to remove.
 *
 * @return  @c true if the string was found and removed.
 */
DENG_PUBLIC boolean StringPool_RemoveById(StringPool* pool, StringPoolId id);

/**
 * Iterate over all strings in the pool making a callback for each. Iteration
 * ends when all strings have been processed or a callback returns non-zero.
 *
 * @param pool      StringPool instance.
 * @param callback  Callback to make for each iteration.
 * @param data      User data to be passed to the callback.
 *
 * @return  @c 0 iff iteration completed wholly. Otherwise the non-zero value returned
 *          by @a callback.
 */
DENG_PUBLIC int StringPool_Iterate(const StringPool* pool, int (*callback)(StringPoolId, void*), void* data);

/**
 * Serializes the pool using @a writer.
 *
 * @param ar StringPool instance.
 * @param writer  Writer instance.
 */
DENG_PUBLIC void StringPool_Write(const StringPool* ar, Writer* writer);

/**
 * Deserializes the pool from @a reader.
 *
 * @param ar StringPool instance.
 * @param reader  Reader instance.
 */
DENG_PUBLIC void StringPool_Read(StringPool* ar, Reader* reader);

#if _DEBUG
/**
 * Print contents of the pool. For debug.
 * @param pool  StringPool instance.
 */
DENG_PUBLIC void StringPool_Print(const StringPool* pool);
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_STRINGPOOL_H */
