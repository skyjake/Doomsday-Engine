/**
 * @file strarray.h
 * Array of text strings. @ingroup base
 *
 * Dynamic, indexable array of text strings.
 *
 * @see stringpool.h for case-insensitive strings
 *
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

#ifndef LIBDENG_STR_ARRAY_H
#define LIBDENG_STR_ARRAY_H

#include "dd_types.h"

struct strarray_s; // opaque

/**
 * StrArray instance. Construct with StrArray_New().
 */
typedef strarray_s StrArray;

/**
 * Constructs an empty string array.
 *
 * @return StrArray instance. Must be deleted with StrArray_Delete().
 */
StrArray* StrArray_New(void);

/**
 * Creates a new sub-array that contains copies of a subset of the
 * array's strings.
 * @param ar  StrArray instance whose strings to copy.
 * @param fromIndex  Start of range of copied strings.
 * @param count  Number of strings in the range. Use -1 to extend to range
 *      to the end of the array.
 *
 * @return  A newly created StrArray instance with copies of the strings.
 *      The returned array will contain @a count strings and
 *      must be deleted with StrArray_Delete().
 */
StrArray* StrArray_NewSub(const StrArray* ar, int fromIndex, int count);

/**
 * Destructs the string array @a ar.
 * @param ar  StrArray instance.
 */
void StrArray_Delete(StrArray* ar);

/**
 * Empties the contents of string array @a ar.
 * @param ar  StrArray instance.
 */
void StrArray_Clear(StrArray* ar);

/**
 * Returns the number of strings in the array.
 * @param ar  StrArray instance.
 */
int StrArray_Size(const StrArray* ar);

/**
 * Appends a string at the end of the array.
 * @param ar  StrArray instance.
 * @param str  Text string to append. A copy is made of the contents.
 */
void StrArray_Append(StrArray* ar, const char* str);

/**
 * Appends an array of text strings at the end of the array.
 * @param ar  StrArray instance.
 * @param other  Another StrArray instance whose strings will be appended
 * to the end of @a ar.
 */
void StrArray_AppendArray(StrArray* ar, const StrArray* other);

/**
 * Inserts a string to the start of the array.
 * @param ar  StrArray instance.
 * @param str  Text string to prepend. A copy is made of the contents.
 */
void StrArray_Prepend(StrArray* ar, const char* str);

/**
 * Inserts a string to the array.
 * @param ar  StrArray instance.
 * @param str  Text string to prepend. A copy is made of the contents.
 * @param atIndex  Position where @a str will appear after the operation
 *      is complete. When inserting at position @em n, strings at positions
 *      <i>n+1..last</i> will be pushed to positions <i>n+2..last+1</i>.
 */
void StrArray_Insert(StrArray* ar, const char* str, int atIndex);

/**
 * Removes the string at position @a index.
 * @param ar  StrArray instance.
 * @param atIndex  Position to remove. When removing position @em n, strings
 *      at positions <i>n+1..last</i> will be pulled to positions <i>n..last-1</i>.
 */
void StrArray_Remove(StrArray* ar, int index);

/**
 * Removes a range of strings from the array.
 * @param ar  StrArray instance.
 * @param fromIndex  Beginning of the range of positions to remove.
 * @param count  Length of the removed range. Use -1 to extend to range
 *      to the end of the array.
 */
void StrArray_RemoveRange(StrArray* ar, int fromIndex, int count);

/**
 * Finds string @a str in the array (case sensitive) and returns its position.
 * @param ar  StrArray instance.
 * @param str  Text string to find.
 *
 * @return Position of the string, or -1 if not found.
 *
 * @note Search operation performance is O(n).
 */
int StrArray_IndexOf(const StrArray* ar, const char* str);

/**
 * Returns a non-modifiable string at position @a index.
 * @param ar  StrArray instance.
 * @param index  Position in the array.
 *
 * @return  Text string.
 */
const char* StrArray_At(const StrArray* ar, int index);

/**
 * Returns a modifiable string at position @a index.
 * @param ar  StrArray instance.
 * @param index  Position in the array.
 *
 * @return  ddstring_t instance that can be modified.
 */
ddstring_t* StrArray_StringAt(StrArray* ar, int index);

/**
 * Checks if the array contains a string (case sensitive).
 * @param ar  StrArray instance.
 * @param str  Text string to check for.
 *
 * @return @c true, if the string is in the array; otherwise @c false.
 *
 * @note Performance is O(n).
 */
boolean StrArray_Contains(const StrArray* ar, const char* str);

/**
 * Serializes the array of strings using @a writer.
 * @param ar StrArray instance.
 * @param writer  Writer instance.
 */
void StrArray_Write(const StrArray* ar, Writer* writer);

/**
 * Deserializes the array of strings from @a reader.
 * @param ar StrArray instance.
 * @param reader  Reader instance.
 */
void StrArray_Read(StrArray* ar, Reader* reader);

#endif // LIBDENG_STR_ARRAY_H
