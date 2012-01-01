/**\file stringpool.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2010-2012 Daniel Swanson <danij@dengine.net>
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
#if _DEBUG
#include "m_misc.h" // For M_NumDigits
#endif

#include "stringpool.h"

// Symbolic value used as identifier for an invalid index in the pool (private).
#define STRINGPOOL_INVALIDINDEX ((uint)-1)

/**
 * @todo Performance is presently suboptimal due to the representation of the
 * intern list. A better implementation would use a binary search tree.
 */
struct stringpool_s
{
    /// Intern list (StringPool::_internsCount size).
    struct stringpool_intern_s* _interns;
    uint _internsCount;

    /// Next unused intern id.
    StringPoolInternId _nextUnusedId;

    /// Number of strings in the pool (not necessarily equal to StringPool::_internsCount).
    uint _numStrings;

    /// String-sorted redirection table (StringPool::_numStrings size).
    uint* _stringSortedInternTable;
};

// Intern string record.
typedef struct stringpool_intern_s {
    ddstring_t string;
    boolean used; // @c true= this intern is currently in use (i.e., it has not been "removed").
} stringpool_intern_t;

static ddstring_t nullString = {"(nullptr)"};

static __inline uint internIdx(StringPool* pool, stringpool_intern_t* intern)
{
    assert(pool && intern);
    return intern - pool->_interns;
}

static __inline StringPoolInternId internId(StringPool* pool, stringpool_intern_t* intern)
{
    assert(pool && intern);
    return internIdx(pool, intern)+1; // 1-based index.
}

static __inline boolean internUsed(StringPool* pool, stringpool_intern_t* intern)
{
    assert(pool);
    return intern->used;
}

static __inline setInternUsed(StringPool* pool, stringpool_intern_t* intern, boolean used)
{
    assert(pool);
    intern->used = used;
}

static __inline stringpool_intern_t* getInternByIdx(StringPool* pool, uint idx)
{
    assert(pool && idx < pool->_internsCount);
    return pool->_interns + idx;
}

static __inline stringpool_intern_t* findInternForId(StringPool* pool, StringPoolInternId id)
{
    if(id != 0 && id <= pool->_internsCount)
        return getInternByIdx(pool, id-1); // ids are 1-based.
    return NULL;
}

static StringPoolInternId findNextUnusedId(StringPool* pool, StringPoolInternId baseId)
{
    stringpool_intern_t* intern = findInternForId(pool, baseId);
    uint i;

    if(intern)
    {
        i = internIdx(pool, intern);
    }
    // We will have to begin our search from the beginning.
    else if(pool->_internsCount)
    {
        intern = getInternByIdx(pool, i = 0);
    }

    if(intern)
    {
        while(internUsed(pool, intern) && ++i < pool->_internsCount)
        {
            intern = getInternByIdx(pool, i);
        }

        if(i != pool->_internsCount)
            return internId(pool, intern);
    }

    // ids are 1-based.
    return (StringPoolInternId) (pool->_internsCount+1);
}

static uint findStringSortedInternIdx(StringPool* pool, const ddstring_t* string)
{
    assert(pool && string);
    {
    uint sortedIdx = STRINGPOOL_INVALIDINDEX; // Not found.
    if(pool->_numStrings)
    {
        uint bottomIdx = 0, topIdx = pool->_numStrings-1;
        stringpool_intern_t* intern;
        boolean found = false;
        int delta;
        while(bottomIdx <= topIdx)
        {
            sortedIdx = bottomIdx + (topIdx - bottomIdx)/2;
            intern = getInternByIdx(pool, pool->_stringSortedInternTable[sortedIdx]);

            delta = Str_CompareIgnoreCase(&intern->string, Str_Text(string));
            if(delta == 0)
            {
                // Found.
                bottomIdx = topIdx+1;
                found = true;
            }
            else if(delta > 0)
            {
                if(sortedIdx == 0)
                {
                    // Not present.
                    bottomIdx = topIdx+1;
                }
                else
                {
                    topIdx = sortedIdx - 1;
                }
            }
            else
            {
                bottomIdx = sortedIdx + 1;
            }
        }

        if(!found) sortedIdx = STRINGPOOL_INVALIDINDEX; // Not found.
    }
    return sortedIdx;
    }
}

static stringpool_intern_t* findIntern(StringPool* pool, const ddstring_t* string)
{
    assert(pool && string);
    {
    uint sortedIdx = findStringSortedInternIdx(pool, string);
    if(sortedIdx == STRINGPOOL_INVALIDINDEX) return NULL;
    return getInternByIdx(pool, pool->_stringSortedInternTable[sortedIdx]);
    }
}

static StringPoolInternId internString(StringPool* pool, const ddstring_t* string)
{
    assert(NULL != pool && NULL != string);
    {
    uint stringSortedMapIdx, idx;
    boolean isNewIntern = true; // @c true= A new intern was allocated.
    stringpool_intern_t* intern;
    StringPoolInternId id;

    if(STRINGPOOL_INVALIDINDEX == pool->_numStrings)
        return 0; // No can do sir, we're out of indices!

    // Obtain a new identifier for this string.
    id = pool->_nextUnusedId;

    if(id > pool->_internsCount)
    {
        // A new intern is required.
        // Add the string to the intern string list.
        pool->_interns = (stringpool_intern_t*) realloc(pool->_interns,
            sizeof *pool->_interns * ++pool->_internsCount);
        if(!pool->_interns)
            Con_Error("StringPool::intern: Failed on (re)allocation of %lu bytes for string list.",
                      (unsigned long) (sizeof *pool->_interns * pool->_internsCount));
        idx = pool->_internsCount-1;
        intern = pool->_interns + idx;

        // Initialize the intern.
        Str_Init(&intern->string);

        // Expand the sorted string index map.
        pool->_stringSortedInternTable = (uint*) realloc(pool->_stringSortedInternTable,
            sizeof *pool->_stringSortedInternTable * pool->_internsCount);
        if(!pool->_stringSortedInternTable)
            Con_Error("StringPool::intern: Failed on (re)allocation of %lu bytes for "
                "intern string-to-index map.", (unsigned long) (sizeof *pool->_stringSortedInternTable * pool->_internsCount));
    }
    else
    {
        // We are reusing an existing intern.
        isNewIntern = false;
        intern = findInternForId(pool, id);
        idx = internIdx(pool, intern);
    }

    // (Re)configure the intern.
    Str_Set(&intern->string, Str_Text(string));
    setInternUsed(pool, intern, true);

    if(isNewIntern)
    {
        pool->_nextUnusedId = id+1; // ids are 1-based.
    }
    else
    {
        pool->_nextUnusedId = findNextUnusedId(pool, id+1); // Begin from the next id.
    }

    // Find the insertion point in the string-sorted intern index table.
    stringSortedMapIdx = 0;
    if(pool->_numStrings)
    {
        uint i;
        for(i = 0; i < pool->_numStrings; ++i)
        {
            const stringpool_intern_t* other = getInternByIdx(pool, pool->_stringSortedInternTable[i]);
            if(Str_CompareIgnoreCase(&other->string, Str_Text(&intern->string)) > 0)
                break;
        }
        if(i != pool->_numStrings)
            memmove(pool->_stringSortedInternTable + i + 1, pool->_stringSortedInternTable + i,
                sizeof(*pool->_stringSortedInternTable) * (pool->_numStrings - i));
        stringSortedMapIdx = i;
    }
    pool->_stringSortedInternTable[stringSortedMapIdx] = idx;

    // There is now one more string in the pool.
    pool->_numStrings += 1;

    return id;
    }
}

static void removeIntern(StringPool* pool, uint sortedIdx)
{
    assert(pool && sortedIdx < pool->_numStrings);
    {
    uint internIdx = pool->_stringSortedInternTable[sortedIdx];
    stringpool_intern_t* intern = getInternByIdx(pool, internIdx);
    StringPoolInternId id = internId(pool, intern);

    // Shift the intern out of the string-sorted intern index table.
    memmove(pool->_stringSortedInternTable + sortedIdx, pool->_stringSortedInternTable + sortedIdx + 1,
            sizeof *pool->_stringSortedInternTable * (pool->_numStrings - sortedIdx - 1));

    if(id < pool->_nextUnusedId)
        pool->_nextUnusedId = id;

    // The intern is no longer in use.
    setInternUsed(pool, intern, false);

    // There is now one less string in the pool.
    pool->_numStrings -= 1;
    }
}

StringPool* StringPool_New(void)
{
    StringPool* pool = (StringPool*) malloc(sizeof *pool);
    if(!pool)
        Con_Error("StringPool::ConstructDefault: Failed on allocation of %lu bytes for "
            "new StringPool.", (unsigned long) sizeof *pool);
    pool->_interns = NULL;
    pool->_internsCount = 0;
    pool->_numStrings = 0;
    pool->_nextUnusedId = 1; // ids are 1-based.
    pool->_stringSortedInternTable = NULL;
    return pool;
}

StringPool* StringPool_NewWithStrings(const ddstring_t* strings, uint count)
{
    StringPool* pool = StringPool_New();
    uint i;
    if(!strings || !count) return pool;
    for(i = 0; i < count; ++i)
    {
        StringPool_Intern(pool, strings + i);
    }
    return pool;
}

void StringPool_Delete(StringPool* pool)
{
    if(!pool) Con_Error("StringPool::Delete: Invalid StringPool");
    StringPool_Clear(pool);
    free(pool);
}

void StringPool_Clear(StringPool* pool)
{
    if(!pool) Con_Error("StringPool::Clear: Invalid StringPool");
    if(0 != pool->_internsCount)
    {
        uint i;
        for(i = 0; i < pool->_internsCount; ++i)
        {
            stringpool_intern_t* intern = &pool->_interns[i];
            Str_Free(&intern->string);
        }
        free(pool->_interns), pool->_interns = NULL;
        free(pool->_stringSortedInternTable), pool->_stringSortedInternTable = NULL;
        pool->_internsCount = 0;
        pool->_numStrings = 0;
        pool->_nextUnusedId = 1; // ids are 1-based.
    }
}

uint StringPool_Size(StringPool* pool)
{
    if(!pool) Con_Error("StringPool::Size: Invalid StringPool");
    return pool->_numStrings;
}

boolean StringPool_Empty(StringPool* pool)
{
    if(!pool) Con_Error("StringPool::Emtpy: Invalid StringPool");
    return ((0 == StringPool_Size(pool))? true:false);
}

StringPoolInternId StringPool_IsInterned(StringPool* pool, const ddstring_t* str)
{
    stringpool_intern_t* intern;
    if(!pool) Con_Error("StringPool::isInterned: Invalid StringPool");
    intern = findIntern(pool, str);
    if(!intern) return 0; // Not found.
    return internId(pool, intern);
}

const ddstring_t* StringPool_String(StringPool* pool, StringPoolInternId internId)
{
    stringpool_intern_t* intern;
    if(!pool) Con_Error("StringPool::String: Invalid StringPool");
    intern = findInternForId(pool, internId);
    if(!intern || !internUsed(pool, intern)) return &nullString;
    return &intern->string;
}

StringPoolInternId StringPool_Intern(StringPool* pool, const ddstring_t* str)
{
    if(!pool) Con_Error("StringPool::Intern: Invalid StringPool");
    if(str)
    {
        stringpool_intern_t* intern = findIntern(pool, str);
        if(!intern)
        {
            // A new string - intern it.
            return internString(pool, str);
        }
        return internId(pool, intern);
    }
    Con_Error("StringPool::Intern: Attempted with null string.");
    exit(1); // Unreachable.
}

const ddstring_t* StringPool_InternAndRetrieve(StringPool* pool, const ddstring_t* str)
{
    if(!pool) Con_Error("StringPool::InternAndRetrieve: Invalid StringPool");
    return StringPool_String(pool, StringPool_Intern(pool, str));
}

boolean StringPool_Remove(StringPool* pool, ddstring_t* str)
{
    uint sortedIdx;

    if(!pool) Con_Error("StringPool::Remove: Invalid StringPool");
    if(!str) return false;

    sortedIdx = findStringSortedInternIdx(pool, str);
    if(sortedIdx == STRINGPOOL_INVALIDINDEX) return false;
    // We are removing an intern.

    if(pool->_numStrings == 1)
    {
        // Removing the only string - take this opportunity to cleanse the whole pool,
        // returning it back to it's initial (empty) state.
        StringPool_Clear(pool);
        return true;
    }

    removeIntern(pool, sortedIdx);
    return true;
}

boolean StringPool_RemoveIntern(StringPool* pool, StringPoolInternId internId)
{
    stringpool_intern_t* intern;
    uint idx, sortedIdx;

    if(!pool) Con_Error("StringPool::RemoveIntern: Invalid StringPool");

    intern = findInternForId(pool, internId);
    if(!intern || !internUsed(pool, intern)) return false;
    // We are removing an intern.

    if(pool->_numStrings == 1)
    {
        // Removing the only string - take this opportunity to cleanse the whole pool,
        // returning it back to it's initial (empty) state.
        StringPool_Clear(pool);
        return true;
    }

    // We need to know the intern's index in the string-sorted table.
    idx = internIdx(pool, intern);
    sortedIdx = 0;
    while(pool->_stringSortedInternTable[sortedIdx] != idx && ++sortedIdx < pool->_numStrings) {}

    // Sanity check.
    assert(sortedIdx != pool->_numStrings);

    removeIntern(pool, sortedIdx);
    return true;
}

#if _DEBUG
void StringPool_Print(StringPool* pool)
{
    int numDigits;
    uint i;

    if(!pool) Con_Error("StringPool::Print: Invalid StringPool");

    numDigits = MAX_OF(M_NumDigits(pool->_numStrings), 3/*length of "idx"*/);
    Con_Printf("StringPool [%p]\n  %*s: string\n", (void*)pool, numDigits, "idx");
    for(i = 0; i < pool->_internsCount; ++i)
    {
        stringpool_intern_t* intern = pool->_interns + i;
        ddstring_t* string = (internUsed(pool, intern)? &intern->string : &nullString);
        Con_Printf("  %*u: %s\n", numDigits, i, Str_Text(string));
    }
    Con_Printf("  There is %u %s in the pool.\n", pool->_numStrings, pool->_internsCount==1? "string":"strings");
}
#endif
