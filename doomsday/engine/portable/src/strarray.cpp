/**
 * @file strarray.cpp
 * Array of text strings. @ingroup base
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

#include "strarray.h"
#include <assert.h>
#include <vector>

namespace de {

/**
 * Minimal C++ wrapper for ddstring_t.
 */
class Str {
public:
    Str(const char* text = 0) {
        Str_InitStd(&str);
        if(text) {
            Str_Set(&str, text);
        }
    }
    ~Str() {
        Str_Free(&str);
    }
    operator ddstring_t* (void) {
        return &str;
    }
    operator const ddstring_t* (void) const {
        return &str;
    }
private:
    ddstring_t str;
};

} // namespace de

struct strarray_s {
    typedef std::vector<de::Str*> Strings;
    Strings array;

    inline void assertValidIndex(int i) const {
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

StrArray* StrArray_New(void)
{
    StrArray* ar = new StrArray;
    return ar;
}

StrArray* StrArray_NewSub(const StrArray *ar, int fromIndex, int count)
{
    assert(ar);
    if(count < 0)
    {
        count = ar->remainder(fromIndex);
    }
    ar->assertValidRange(fromIndex, count);

    StrArray* sub = StrArray_New();
    for(int i = 0; i < count; ++i)
    {
        StrArray_Append(sub, StrArray_At(ar, fromIndex + i));
    }
    return sub;
}

void StrArray_Delete(StrArray* ar)
{
    if(ar) delete ar;
}

void StrArray_Clear(StrArray* ar)
{
    assert(ar);
    for(StrArray::Strings::iterator i = ar->array.begin(); i != ar->array.end(); ++i)
    {
        delete *i;
    }
    ar->array.clear();
}

int StrArray_Size(const StrArray *ar)
{
    assert(ar);
    return ar->array.size();
}

void StrArray_Append(StrArray* ar, const char* str)
{
    assert(ar);
    ar->array.push_back(new de::Str(str));
}

void StrArray_AppendArray(StrArray* ar, const StrArray* other)
{
    assert(ar);
    assert(other);
    assert(ar != other);
    for(StrArray::Strings::const_iterator i = other->array.begin(); i != other->array.end(); ++i)
    {
        StrArray_Append(ar, Str_Text(**i));
    }
}

void StrArray_Prepend(StrArray* ar, const char* str)
{
    StrArray_Insert(ar, str, 0);
}

void StrArray_Insert(StrArray* ar, const char* str, int atIndex)
{
    assert(ar);
    ar->assertValidIndex(atIndex);
    ar->array.insert(ar->array.begin() + atIndex, new de::Str(str));
}

void StrArray_Remove(StrArray* ar, int index)
{
    assert(ar);
    ar->assertValidIndex(index);
    delete ar->array[index];
    ar->array.erase(ar->array.begin() + index);
}

void StrArray_RemoveRange(StrArray* ar, int fromIndex, int count)
{
    if(count < 0)
    {
        count = ar->remainder(fromIndex);
    }
    for(int i = 0; i < count; ++i)
    {
        StrArray_Remove(ar, fromIndex);
    }
}

int StrArray_IndexOf(const StrArray *ar, const char* str)
{
    assert(ar);
    for(uint i = 0; i < ar->array.size(); ++i)
    {
        if(!strcmp(str, Str_Text(*ar->array[i])))
            return i;
    }
    return -1;
}

const char* StrArray_At(const StrArray *ar, int index)
{
    assert(ar);
    ar->assertValidIndex(index);
    return Str_Text(*ar->array[index]);
}

ddstring_t* StrArray_StringAt(StrArray* ar, int index)
{
    assert(ar);
    ar->assertValidIndex(index);
    return *ar->array[index];
}

boolean StrArray_Contains(const StrArray* ar, const char* str)
{
    return StrArray_IndexOf(ar, str) >= 0;
}

void StrArray_Write(const StrArray *ar, Writer* writer)
{
    assert(ar);
    Writer_WriteUInt32(writer, ar->array.size());
    // Write each of the strings.
    for(StrArray::Strings::const_iterator i = ar->array.begin(); i != ar->array.end(); ++i)
    {
        Str_Write(**i, writer);
    }
}

void StrArray_Read(StrArray* ar, Reader* reader)
{
    StrArray_Clear(ar);

    uint count = Reader_ReadUInt32(reader);
    for(uint i = 0; i < count; ++i)
    {
        de::Str* str = new de::Str;
        Str_Read(*str, reader);
        ar->array.push_back(str);
    }
}
