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
        assert(i < (int)array.size());
    }

    inline void assertValidRange(int from, int count) const {
        assertValidIndex(from);
        assertValidIndex(from + count - 1);
    }
};

StrArray* StrArray_New(void)
{
    StrArray* sar = new StrArray;
    return sar;
}

void StrArray_Delete(StrArray* sar)
{
    if(sar) delete sar;
}

void StrArray_Clear(StrArray* sar)
{
    assert(sar);
    for(StrArray::Strings::iterator i = sar->array.begin(); i != sar->array.end(); ++i)
    {
        delete *i;
    }
    sar->array.clear();
}

int StrArray_Size(StrArray* sar)
{
    assert(sar);
    return sar->array.size();
}

void StrArray_Append(StrArray* sar, const char* str)
{
    assert(sar);
    sar->array.push_back(new de::Str(str));
}

void StrArray_AppendArray(StrArray* sar, const StrArray* other)
{
    assert(sar);
    assert(other);
    for(StrArray::Strings::const_iterator i = other->array.begin(); i != other->array.end(); ++i)
    {
        StrArray_Append(sar, Str_Text(**i));
    }
}

void StrArray_Prepend(StrArray* sar, const char* str)
{
    StrArray_Insert(sar, str, 0);
}

void StrArray_Insert(StrArray* sar, const char* str, int atIndex)
{
    assert(sar);
    sar->assertValidIndex(atIndex);
    sar->array.insert(sar->array.begin() + atIndex, new de::Str(str));
}

void StrArray_Remove(StrArray* sar, int index)
{
    assert(sar);
    sar->assertValidIndex(index);
    delete sar->array[index];
    sar->array.erase(sar->array.begin() + index);
}

void StrArray_RemoveRange(StrArray* sar, int fromIndex, int count)
{
    for(int i = 0; i < count; ++i)
    {
        StrArray_Remove(sar, fromIndex);
    }
}

StrArray* StrArray_Sub(StrArray* sar, int fromIndex, int count)
{
    assert(sar);
    sar->assertValidRange(fromIndex, count);

    StrArray* sub = StrArray_New();
    for(int i = 0; i < count; ++i)
    {
        StrArray_Append(sub, StrArray_At(sar, fromIndex + i));
    }
    return sub;
}

int StrArray_IndexOf(StrArray* sar, const char* str)
{
    assert(sar);
    for(uint i = 0; i < sar->array.size(); ++i)
    {
        if(!strcmp(str, Str_Text(*sar->array[i])))
            return i;
    }
    return -1;
}

const char* StrArray_At(StrArray* sar, int index)
{
    assert(sar);
    sar->assertValidIndex(index);
    return Str_Text(*sar->array[index]);
}

boolean StrArray_Contains(StrArray* sar, const char* str)
{
    return StrArray_IndexOf(sar, str) >= 0;
}

void StrArray_Write(StrArray* sar, Writer* writer)
{
    assert(sar);
    Writer_WriteUInt32(writer, sar->array.size());
    // Write each of the strings.
    for(StrArray::Strings::const_iterator i = sar->array.begin(); i != sar->array.end(); ++i)
    {
        Str_Write(**i, writer);
    }
}

void StrArray_Read(StrArray* sar, Reader* reader)
{
    StrArray_Clear(sar);

    uint count = Reader_ReadUInt32(reader);
    for(uint i = 0; i < count; ++i)
    {
        de::Str* str = new de::Str;
        Str_Read(*str, reader);
        sar->array.push_back(str);
    }
}
