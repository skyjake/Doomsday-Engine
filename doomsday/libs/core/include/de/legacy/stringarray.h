/**
 * @file stringarray.h
 * Array of text strings.
 *
 * Dynamic, indexable array of text strings. The functionality of this class is
 * comparable to QStringList, however with a pure C API.
 *
 * @todo Use StringPool internally for efficient storage, searches
 *
 * @authors Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DE_STRING_ARRAY_H
#define DE_STRING_ARRAY_H

#include "types.h"
#include "str.h"

#ifdef __cplusplus
extern "C" {
#endif

/// @addtogroup legacyData
/// @{

struct stringarray_s; // opaque

/**
 * StringArray instance. Construct with StringArray_New().
 */
typedef struct stringarray_s StringArray;

/**
 * Constructs an empty string array.
 *
 * @return StringArray instance. Must be deleted with StringArray_Delete().
 */
DE_PUBLIC StringArray *StringArray_New(void);

/**
 * Creates a new sub-array that contains copies of a subset of the
 * array's strings.
 * @param ar  StringArray instance whose strings to copy.
 * @param fromIndex  Start of range of copied strings.
 * @param count  Number of strings in the range. Use -1 to extend to range
 *      to the end of the array.
 *
 * @return  A newly created StringArray instance with copies of the strings.
 *      The returned array will contain @a count strings and
 *      must be deleted with StringArray_Delete().
 */
DE_PUBLIC StringArray *StringArray_NewSub(const StringArray *ar, int fromIndex, int count);

/**
 * Destructs the string array @a ar.
 * @param ar  StringArray instance.
 */
DE_PUBLIC void StringArray_Delete(StringArray *ar);

/**
 * Empties the contents of string array @a ar.
 * @param ar  StringArray instance.
 */
DE_PUBLIC void StringArray_Clear(StringArray *ar);

/**
 * Returns the number of strings in the array.
 * @param ar  StringArray instance.
 */
DE_PUBLIC int StringArray_Size(const StringArray *ar);

/**
 * Appends a string at the end of the array.
 * @param ar  StringArray instance.
 * @param str  Text string to append. A copy is made of the contents.
 */
DE_PUBLIC void StringArray_Append(StringArray *ar, const char *str);

/**
 * Appends an array of text strings at the end of the array.
 * @param ar  StringArray instance.
 * @param other  Another StringArray instance whose strings will be appended
 * to the end of @a ar.
 */
DE_PUBLIC void StringArray_AppendArray(StringArray *ar, const StringArray *other);

/**
 * Inserts a string to the start of the array.
 * @param ar  StringArray instance.
 * @param str  Text string to prepend. A copy is made of the contents.
 */
DE_PUBLIC void StringArray_Prepend(StringArray *ar, const char *str);

/**
 * Inserts a string to the array.
 * @param ar  StringArray instance.
 * @param str  Text string to prepend. A copy is made of the contents.
 * @param atIndex  Position where @a str will appear after the operation
 *      is complete. When inserting at position @em n, strings at positions
 *      <i>n+1..last</i> will be pushed to positions <i>n+2..last+1</i>.
 */
DE_PUBLIC void StringArray_Insert(StringArray *ar, const char *str, int atIndex);

/**
 * Removes the string at position @a index.
 * @param ar  StringArray instance.
 * @param index  Position to remove. When removing position @em n, strings
 *      at positions <i>n+1..last</i> will be pulled to positions <i>n..last-1</i>.
 */
DE_PUBLIC void StringArray_Remove(StringArray *ar, int index);

/**
 * Removes a range of strings from the array.
 * @param ar  StringArray instance.
 * @param fromIndex  Beginning of the range of positions to remove.
 * @param count  Length of the removed range. Use -1 to extend to range
 *      to the end of the array.
 */
DE_PUBLIC void StringArray_RemoveRange(StringArray *ar, int fromIndex, int count);

/**
 * Finds string @a str in the array (case sensitive) and returns its position.
 * @param ar  StringArray instance.
 * @param str  Text string to find.
 *
 * @return Position of the string, or -1 if not found.
 *
 * @note Search operation performance is O(n).
 */
DE_PUBLIC int StringArray_IndexOf(const StringArray *ar, const char *str);

/**
 * Returns a non-modifiable string at position @a index.
 * @param ar  StringArray instance.
 * @param index  Position in the array.
 *
 * @return  Text string.
 */
DE_PUBLIC const char *StringArray_At(const StringArray *ar, int index);

/**
 * Returns a modifiable string at position @a index.
 * @param ar  StringArray instance.
 * @param index  Position in the array.
 *
 * @return  ddstring_t instance that can be modified.
 */
DE_PUBLIC Str *StringArray_StringAt(StringArray *ar, int index);

/**
 * Checks if the array contains a string (case sensitive).
 * @param ar  StringArray instance.
 * @param str  Text string to check for.
 *
 * @return @c true, if the string is in the array; otherwise @c false.
 *
 * @note Performance is O(n).
 */
DE_PUBLIC dd_bool StringArray_Contains(const StringArray *ar, const char *str);

/**
 * Serializes the array of strings using @a writer.
 * @param ar StringArray instance.
 * @param writer  Writer1 instance.
 */
DE_PUBLIC void StringArray_Write(const StringArray *ar, Writer1 *writer);

/**
 * Deserializes the array of strings from @a reader.
 * @param ar StringArray instance.
 * @param reader  Reader1 instance.
 */
DE_PUBLIC void StringArray_Read(StringArray *ar, Reader1 *reader);

/// @}

#ifdef __cplusplus
} // extern "C"
#endif

#endif // DE_STRING_ARRAY_H
