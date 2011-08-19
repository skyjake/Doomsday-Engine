/**\file stringpool.c
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

#include "de_base.h"
#include "de_console.h"

#include "stringpool.h"

// Intern string record.
typedef struct stringpool_intern_s {
    ddstring_t string;
} stringpool_intern_t;

static stringpool_intern_t* internById(stringpool_t* pool, stringpool_internid_t internId)
{
    assert(NULL != pool);
    if(0 != internId && internId <= pool->_internsCount)
        return &pool->_interns[internId-1];
    Con_Error("StringPool::internById: Invalid id %u.", internId);
    exit(1); // Unreachable.
}

static stringpool_internid_t findInternId(stringpool_t* pool, const ddstring_t* string)
{
    assert(NULL != pool && NULL != string);
    {
    stringpool_internid_t internId = 0; // Not a valid id.
    if(0 != pool->_internsCount)
    {
        uint bottomIdx = 0, topIdx = pool->_internsCount - 1, pivot;
        const stringpool_intern_t* intern;
        boolean isDone = false;
        int result;
        while(bottomIdx <= topIdx && !isDone)
        {
            pivot = bottomIdx + (topIdx - bottomIdx)/2;
            intern = internById(pool, pool->_sortedInternTable[pivot]);

            result = Str_CompareIgnoreCase(&intern->string, Str_Text(string));
            if(result == 0)
            {   // Found.
                internId = pool->_sortedInternTable[pivot];
                isDone = true;
            }
            else if(result > 0)
            {
                if(pivot == 0)
                    isDone = true; // Not present.
                else
                    topIdx = pivot - 1;
            }
            else
            {
                bottomIdx = pivot + 1;
            }
        }
    }
    return internId;
    }
}

static stringpool_internid_t intern(stringpool_t* pool, const ddstring_t* string)
{
    assert(NULL != pool && NULL != string);
    {
    stringpool_intern_t* intern;
    uint sortedMapIndex = 0;

    if(((uint)-1) == pool->_internsCount)
        return 0; // No can do sir, we're out of indices!

    // Add the string to the intern string list.
    pool->_interns = (stringpool_intern_t*) realloc(pool->_interns,
        sizeof(*pool->_interns) * ++pool->_internsCount);
    if(NULL == pool->_interns)
        Con_Error("StringPool::intern: Failed on (re)allocation of %lu bytes for string list.",
            (unsigned long) (sizeof(*pool->_interns) * pool->_internsCount));

    intern = &pool->_interns[pool->_internsCount-1];
    Str_Init(&intern->string);
    Str_Set(&intern->string, Str_Text(string));

    // Update the intern string identifier map.
    pool->_sortedInternTable = (stringpool_internid_t*) realloc(pool->_sortedInternTable,
        sizeof(*pool->_sortedInternTable) * pool->_internsCount);
    if(NULL == pool->_sortedInternTable)
        Con_Error("StringPool::intern: Failed on (re)allocation of %lu bytes for "
            "intern string identifier map.", (unsigned long) (sizeof(*pool->_sortedInternTable) * pool->_internsCount));

    // Find the insertion point.
    if(1 != pool->_internsCount)
    {
        uint i;
        for(i = 0; i < pool->_internsCount - 1; ++i)
        {
            const stringpool_intern_t* other = internById(pool, pool->_sortedInternTable[i]);
            if(Str_CompareIgnoreCase(&other->string, Str_Text(&intern->string)) > 0)
                break;
        }
        if(i != pool->_internsCount - 1)
            memmove(pool->_sortedInternTable + i + 1, pool->_sortedInternTable + i,
                sizeof(*pool->_sortedInternTable) * (pool->_internsCount - 1 - i));
        sortedMapIndex = i;
    }
    pool->_sortedInternTable[sortedMapIndex] = pool->_internsCount; // 1-based index.

    return pool->_internsCount; // 1-based index.
    }
}

stringpool_t* StringPool_New(void)
{
    stringpool_t* pool = (stringpool_t*) malloc(sizeof(*pool));
    if(NULL == pool)
        Con_Error("StringPool::ConstructDefault: Failed on allocation of %lu bytes for "
            "new StringPool.", (unsigned long) sizeof(*pool));
    pool->_interns = NULL;
    pool->_internsCount = 0;
    pool->_sortedInternTable = NULL;
    return pool;
}

stringpool_t* StringPool_NewWithStrings(ddstring_t** strings, uint count)
{
    stringpool_t* pool = StringPool_New();
    uint i;
    if(NULL == strings || 0 == count) return pool;

    for(i = 0; i < count; ++i)
    {
        StringPool_Intern(pool, strings[i]);
    }
    return pool;
}

void StringPool_Delete(stringpool_t* pool)
{
    assert(NULL != pool);
    StringPool_Clear(pool);
    free(pool);
}

void StringPool_Clear(stringpool_t* pool)
{
    assert(NULL != pool);
    if(0 != pool->_internsCount)
    {
        uint i;
        for(i = 0; i < pool->_internsCount; ++i)
        {
            stringpool_intern_t* intern = &pool->_interns[i];
            Str_Free(&intern->string);
        }
        free(pool->_interns), pool->_interns = NULL;
        free(pool->_sortedInternTable), pool->_sortedInternTable = NULL;
        pool->_internsCount = 0;
    }
}

uint StringPool_Size(stringpool_t* pool)
{
    assert(pool);
    return pool->_internsCount;
}

boolean StringPool_Empty(stringpool_t* pool)
{
    return (0 == StringPool_Size(pool));
}

stringpool_internid_t StringPool_IsInterned(stringpool_t* pool, const ddstring_t* str)
{
    return findInternId(pool, str);
}

const ddstring_t* StringPool_String(stringpool_t* pool, stringpool_internid_t internId)
{
    return &internById(pool, internId)->string;
}

stringpool_internid_t StringPool_Intern(stringpool_t* pool, const ddstring_t* str)
{
    if(!(NULL == str || Str_IsEmpty(str)))
    {
        stringpool_internid_t internId = StringPool_IsInterned(pool, str);
        if(0 == internId)
        {
            return intern(pool, str);
        }
        return internId;
    }
    Con_Error("StringPool::Intern: Attempted with zero-length/null string.");
    exit(1); // Unreachable.
}

const ddstring_t* StringPool_InternAndRetrieve(stringpool_t* pool, const ddstring_t* str)
{
    return StringPool_String(pool, StringPool_Intern(pool, str));
}
