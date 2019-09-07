/**
 * @file stringarray.cpp
 * Array of text strings implementation.
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

#include "de/legacy/stringarray.h"
#include "de/legacy/str.hh"
#include <assert.h>
#include <vector>
#include <string.h>

struct stringarray_s {
    std::vector<de::Str *> array;

    inline void assertValidIndex(int DE_DEBUG_ONLY(i)) const {
        assert(i >= 0);
        assert(i < int(array.size()));
    }

    inline void assertValidRange(int from, int count) const {
        assertValidIndex(from);
        assertValidIndex(from + count - 1);
    }

    inline int remainder(int from) const {
        return int(array.size()) - from;
    }
};

StringArray *StringArray_New(void)
{
    StringArray *ar = new StringArray;
    return ar;
}

StringArray *StringArray_NewSub(const StringArray *ar, int fromIndex, int count)
{
    assert(ar);
    if (count < 0)
    {
        count = ar->remainder(fromIndex);
    }
    ar->assertValidRange(fromIndex, count);

    StringArray *sub = StringArray_New();
    for (int i = 0; i < count; ++i)
    {
        StringArray_Append(sub, StringArray_At(ar, fromIndex + i));
    }
    return sub;
}

void StringArray_Delete(StringArray *ar)
{
    if (ar) delete ar;
}

void StringArray_Clear(StringArray *ar)
{
    assert(ar);
    for (auto i = ar->array.begin(); i != ar->array.end(); ++i)
    {
        delete *i;
    }
    ar->array.clear();
}

int StringArray_Size(const StringArray *ar)
{
    assert(ar);
    return ar->array.size();
}

void StringArray_Append(StringArray *ar, const char *str)
{
    assert(ar);
    ar->array.push_back(new de::Str(str));
}

void StringArray_AppendArray(StringArray *ar, const StringArray *other)
{
    assert(ar);
    assert(other);
    assert(ar != other);
    for (auto i = other->array.begin(); i != other->array.end(); ++i)
    {
        StringArray_Append(ar, Str_Text(**i));
    }
}

void StringArray_Prepend(StringArray *ar, const char *str)
{
    StringArray_Insert(ar, str, 0);
}

void StringArray_Insert(StringArray *ar, const char *str, int atIndex)
{
    assert(ar);
    ar->assertValidIndex(atIndex);
    ar->array.insert(ar->array.begin() + atIndex, new de::Str(str));
}

void StringArray_Remove(StringArray *ar, int index)
{
    assert(ar);
    ar->assertValidIndex(index);
    delete ar->array[index];
    ar->array.erase(ar->array.begin() + index);
}

void StringArray_RemoveRange(StringArray *ar, int fromIndex, int count)
{
    if (count < 0)
    {
        count = ar->remainder(fromIndex);
    }
    for (int i = 0; i < count; ++i)
    {
        StringArray_Remove(ar, fromIndex);
    }
}

int StringArray_IndexOf(const StringArray *ar, const char *str)
{
    assert(ar);
    for (uint i = 0; i < ar->array.size(); ++i)
    {
        if (!strcmp(str, Str_Text(*ar->array[i])))
            return i;
    }
    return -1;
}

const char *StringArray_At(const StringArray *ar, int index)
{
    assert(ar);
    ar->assertValidIndex(index);
    return Str_Text(*ar->array[index]);
}

Str *StringArray_StringAt(StringArray *ar, int index)
{
    assert(ar);
    ar->assertValidIndex(index);
    return *ar->array[index];
}

dd_bool StringArray_Contains(const StringArray *ar, const char *str)
{
    return StringArray_IndexOf(ar, str) >= 0;
}

void StringArray_Write(const StringArray *ar, Writer1 *writer)
{
    assert(ar);
    Writer_WriteUInt32(writer, ar->array.size());
    // Write each of the strings.
    for (auto i = ar->array.begin(); i != ar->array.end(); ++i)
    {
        Str_Write(**i, writer);
    }
}

void StringArray_Read(StringArray *ar, Reader1 *reader)
{
    StringArray_Clear(ar);

    uint count = Reader_ReadUInt32(reader);
    for (uint i = 0; i < count; ++i)
    {
        de::Str *str = new de::Str;
        Str_Read(*str, reader);
        ar->array.push_back(str);
    }
}
