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

StrArray* StrArray_New(void);

void StrArray_Delete(StrArray* sar);

void StrArray_Clear(StrArray* sar);

int StrArray_Size(StrArray* sar);

void StrArray_Append(StrArray* sar, const char* str);

void StrArray_AppendArray(StrArray* sar, const StrArray* other);

void StrArray_Prepend(StrArray* sar, const char* str);

void StrArray_Insert(StrArray* sar, const char* str, int atIndex);

void StrArray_Remove(StrArray* sar, int index);

void StrArray_RemoveRange(StrArray* sar, int fromIndex, int count);

/**
 * @return  A newly created StrArray instance with copies of the strings.
 */
StrArray* StrArray_Sub(StrArray* sar, int fromIndex, int count);

int StrArray_IndexOf(StrArray* sar, const char* str);

const char* StrArray_At(StrArray* sar, int index);

boolean StrArray_Contains(StrArray* sar, const char* str);

void StrArray_Write(StrArray* sar, Writer* writer);

void StrArray_Read(StrArray* sar, Reader* reader);

#endif // LIBDENG_STR_ARRAY_H
