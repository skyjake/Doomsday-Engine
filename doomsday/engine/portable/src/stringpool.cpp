/**
 * @file stringpool.cpp
 * String pool (case insensitive) implementation. @ingroup base
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

//#include "de_base.h"
//#include "de_console.h"
//#if _DEBUG
//#  include "m_misc.h" // for M_NumDigits
//#endif

#include "de_platform.h"
#include "stringpool.h"
#include "string.hh"
#include "unittest.h"

#include <vector>
#include <list>
#include <set>
#include <algorithm>

/// Macro used for converting internal ids to externally visible StringPoolIds.
#define EXPORT_ID(i)    (uint(i) + 1)
#define IMPORT_ID(i)    (StringPoolId((i) - 1))

static const de::Str nullString = "(nullptr)";

typedef uint InternalId;

/**
 * Case-insensitive reference to a text string (ddstring).
 * Does not take copies or retain ownership to the string.
 */
class CaselessStr
{
public:
    CaselessStr(const char* text = 0) : _id(0), _userValue(0) {
        setText(text);
    }
    CaselessStr(const CaselessStr& other) : _id(other._id), _userValue(other._userValue) {
        setText(other._str.str);
    }
    void setText(const char* text) {
        Str_InitStatic(&_str, text);
    }
    char* toCString() const {
        return const_cast<char*>(_str.str);
    }
    operator const char* () const {
        return _str.str;
    }
    operator const ddstring_t* () const {
        return &_str;
    }
    InternalId id() const {
        return _id;
    }
    void setId(InternalId i) {
        _id = i;
    }
    uint userValue() const {
        return _userValue;
    }
    void setUserValue(uint value) {
        _userValue = value;
    }
    void serialize(Writer* writer) const {
        Str_Write(&_str, writer);
        Writer_WritePackedUInt32(writer, _id);
        Writer_WriteUInt32(writer, _userValue);
    }
    void deserialize(Reader* reader, ddstring_t* text) {
        Str_Read(text, reader);
        _str.str = 0; // caller must handle this; we don't take ownership
        _id = Reader_ReadPackedUInt32(reader);
        _userValue = Reader_ReadUInt32(reader);
    }
private:
    ddstring_t _str;
    InternalId _id; ///< The id that refers to this string.
    uint _userValue;
};

class CaselessStrRef {
public:
    CaselessStrRef(const CaselessStr* s = 0) {
        _str = s;
    }
    CaselessStrRef(const CaselessStrRef& other) {
        _str = other._str;
    }
    CaselessStr* toStr() const {
        return const_cast<CaselessStr*>(_str);
    }
    InternalId id() const {
        assert(_str);
        return _str->id();
    }
    bool operator < (const CaselessStrRef& other) const {
        assert(_str);
        assert(other._str);
        return stricmp(*_str, *other._str) < 0;
    }
    bool operator == (const CaselessStrRef& other) const {
        assert(_str);
        assert(other._str);
        return !stricmp(*_str, *other._str);
    }
private:
    const CaselessStr* _str;
};

typedef std::set<CaselessStrRef> Interns;
typedef std::vector<CaselessStr*> IdMap;
typedef std::list<InternalId> AvailableIds;

struct stringpool_s {
    /// Interned strings (owns the CaselessStr instances).
    Interns interns;

    /// InternId => CaselessStr*. Only one id can refer to the each CaselessStr*.
    IdMap idMap;

    /// Number of strings in the pool (must always be idMap.size() - available.size()).
    size_t count;

    /// List of currently unused ids in idMap.
    AvailableIds available;

    stringpool_s() : count(0)
    {}

    ~stringpool_s()
    {
        // Free all allocated memory.
        clear();
    }

    void serialize(Writer* writer) const
    {
        // Number of strings altogether (includes unused ids).
        Writer_WriteUInt32(writer, idMap.size());

        // Write the interns.
        Writer_WriteUInt32(writer, interns.size());
        for(Interns::const_iterator i = interns.begin(); i != interns.end(); ++i)
        {
            i->toStr()->serialize(writer);
        }
    }

    void deserialize(Reader* reader)
    {
        clear();

        uint numStrings = Reader_ReadUInt32(reader);
        idMap.resize(numStrings, 0);

        uint numInterns = Reader_ReadUInt32(reader);
        while(numInterns--)
        {
            ddstring_t text;
            Str_InitStd(&text);

            CaselessStr* str = new CaselessStr;
            str->deserialize(reader, &text);
            // Create a copy of the string whose ownership StringPool controls.
            str->setText(strdup(Str_Text(&text)));
            interns.insert(str);
            Str_Free(&text);

            // Update the id map.
            idMap[str->id()] = str;

            count++;
        }

        // Update the available ids.
        for(uint i = 0; i < idMap.size(); ++i)
        {
            if(!idMap[i]) available.push_back(i);
        }

        assertCount();
    }

    void inline assertCount() const
    {
        assert(count == interns.size());
        assert(count == idMap.size() - available.size());
    }

    void clear()
    {
        for(uint i = 0; i < idMap.size(); ++i)
        {
            if(!idMap[i]) continue; // Unused slot.
            destroyStr(idMap[i]);
        }
        count = 0;
        interns.clear();
        idMap.clear();
        available.clear();

        assertCount();
    }

    Interns::iterator findIntern(const char* text)
    {
        const CaselessStr key(text);
        return interns.find(CaselessStrRef(&key)); // O(log n)
    }

    Interns::const_iterator findIntern(const char* text) const
    {
        const CaselessStr key(text);
        return interns.find(CaselessStrRef(&key)); // O(log n)
    }

    /**
     * Before this is called make sure there is no duplicate of @a text in the
     * interns set.
     *
     * @param text  Text string to add to the interned strings.
     *              A copy is made of this.
     */
    InternalId copyAndAssignUniqueId(const char* text)
    {
        CaselessStr* str = new CaselessStr(strdup(text));

        // This is a new string that is added to the pool.
        interns.insert(str); // O(log n)

        return assignUniqueId(str);
    }

    void destroyStr(CaselessStr* str)
    {
        free(str->toCString()); // duplicated cstring
        delete str; // CaselessStr instance
    }

    InternalId assignUniqueId(CaselessStr* str) // O(1)
    {
        InternalId idx;

        // Any available ids in the shortlist?
        if(!available.empty()) // O(1)
        {
            idx = available.front();
            available.pop_front();
            idMap[idx] = str;
        }
        else
        {
            // Expand the idMap.
            idx = idMap.size();
            idMap.push_back(str); // O(1) (amortized)
        }
        str->setId(idx);

        // We have one more logical string in the pool.
        count++;
        assertCount();

        return idx;
    }

    void releaseAndDestroy(InternalId id, Interns::iterator* iterToErase = 0)
    {
        assert(id < idMap.size());

        CaselessStr* interned = idMap[id];
        assert(interned != 0);

        idMap[id] = 0;
        available.push_back(id);

        // Delete the string itself, no one refers to it any more.
        destroyStr(interned);

        // If the caller already located the interned string, let's use it
        // to erase the string in O(1) time. Otherwise it's up to the
        // caller to make sure it gets removed from the interns.
        if(iterToErase) interns.erase(*iterToErase); // O(1) (amortized)

        // One less string.
        count--;
        assertCount();
    }
};

StringPool* StringPool_New(void)
{
    StringPool* pool = new StringPool;
    return pool;
}

StringPool* StringPool_NewWithStrings(const ddstring_t* strings, uint count)
{
    StringPool* pool = StringPool_New();
    for(uint i = 0; strings && i < count; ++i)
    {
        StringPool_Intern(pool, strings + i);
    }
    return pool;
}

void StringPool_Delete(StringPool* pool)
{
    assert(pool);
    delete pool;
}

void StringPool_Clear(StringPool* pool)
{
    assert(pool);
    pool->clear();
}

boolean StringPool_Empty(const StringPool* pool)
{
    assert(pool);
    pool->assertCount();
    return !pool->count;
}

uint StringPool_Size(const StringPool* pool)
{
    assert(pool);
    pool->assertCount();
    return uint(pool->count);
}

StringPoolId StringPool_Intern(StringPool* pool, const ddstring_t* str)
{
    assert(pool);
    Interns::iterator found = pool->findIntern(Str_Text(str)); // O(log n)
    if(found != pool->interns.end())
    {
        // Already got this one.
        return EXPORT_ID(found->id());
    }
    return EXPORT_ID(pool->copyAndAssignUniqueId(Str_Text(str))); // O(log n)
}

const ddstring_t* StringPool_InternAndRetrieve(StringPool* pool, const ddstring_t* str)
{
    assert(pool);
    InternalId id = IMPORT_ID(StringPool_Intern(pool, str));
    return *pool->idMap[id];
}

void StringPool_SetUserValue(StringPool* pool, StringPoolId id, uint value)
{
    assert(pool);
    const InternalId internalId = IMPORT_ID(id);
    assert(internalId < pool->idMap.size());
    assert(pool->idMap[internalId] != 0);
    pool->idMap[internalId]->setUserValue(value); // O(1)
}

uint StringPool_UserValue(StringPool* pool, StringPoolId id)
{
    assert(pool);
    const InternalId internalId = IMPORT_ID(id);
    assert(internalId < pool->idMap.size());
    assert(pool->idMap[internalId] != 0);
    return pool->idMap[internalId]->userValue(); // O(1)
}

StringPoolId StringPool_IsInterned(const StringPool* pool, const ddstring_t* str)
{
    assert(pool);
    Interns::const_iterator found = pool->findIntern(Str_Text(str)); // O(log n)
    if(found != pool->interns.end())
    {
        return EXPORT_ID(found->id());
    }
    // Not found.
    return 0;
}

const ddstring_t* StringPool_String(const StringPool* pool, StringPoolId id)
{
    assert(pool);
    return *pool->idMap[IMPORT_ID(id)];
}

boolean StringPool_Remove(StringPool* pool, const ddstring_t* str)
{
    assert(pool);
    Interns::iterator found = pool->findIntern(Str_Text(str)); // O(log n)
    if(found != pool->interns.end())
    {
        pool->releaseAndDestroy(found->id(), &found); // O(1) (amortized)
        return true;
    }
    return false;
}

boolean StringPool_RemoveById(StringPool* pool, StringPoolId id)
{
    assert(pool);

    if(id >= pool->idMap.size()) return false;
    CaselessStr* str = pool->idMap[IMPORT_ID(id)];
    if(!str) return false;

    pool->interns.erase(str); // O(log n)
    pool->releaseAndDestroy(str->id());
    return true;
}

int StringPool_Iterate(const StringPool* pool, int (*callback)(StringPoolId, void*), void* data)
{
    assert(pool);
    if(!callback) return 0;
    for(uint i = 0; i < pool->idMap.size(); ++i)
    {
        if(!pool->idMap[i]) continue;
        int result = callback(EXPORT_ID(i), data);
        if(result) return result;
    }
    return 0;
}

void StringPool_Write(const StringPool* pool, Writer* writer)
{
    assert(pool);
    pool->serialize(writer);
}

void StringPool_Read(StringPool* pool, Reader* reader)
{
    assert(pool);
    pool->deserialize(reader);
}


#if 0

// Intern string record.
typedef struct stringpool_intern_s {
    boolean used; // @c true= this intern is currently in use (i.e., it has not been "removed").
    ddstring_t string;
} stringpool_intern_t;

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

static ddstring_t nullString = {"(nullptr)"};

static __inline uint internIdx(const StringPool* pool, stringpool_intern_t* intern)
{
    assert(pool && intern);
    return intern - pool->_interns;
}

static __inline StringPoolInternId internId(const StringPool* pool, stringpool_intern_t* intern)
{
    assert(pool && intern);
    return internIdx(pool, intern)+1; // 1-based index.
}

static __inline boolean internUsed(const StringPool* pool, stringpool_intern_t* intern)
{
    assert(pool);
    return intern->used;
}

static __inline setInternUsed(StringPool* pool, stringpool_intern_t* intern, boolean used)
{
    assert(pool);
    intern->used = used;
}

static __inline stringpool_intern_t* getInternByIdx(const StringPool* pool, uint idx)
{
    assert(pool && idx < pool->_internsCount);
    return pool->_interns + idx;
}

static __inline stringpool_intern_t* findInternForId(const StringPool* pool, StringPoolInternId id)
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

static boolean findStringSortedInternIdx(const StringPool* pool,
    const ddstring_t* string, uint* result)
{
    assert(pool && string);
    if(pool->_numStrings)
    {
        uint pivot, bottomIdx = 0, topIdx = pool->_numStrings-1;
        stringpool_intern_t* intern;
        boolean found = false;
        int delta;

        while(bottomIdx <= topIdx)
        {
            pivot = bottomIdx + (topIdx - bottomIdx)/2;
            intern = getInternByIdx(pool, pool->_stringSortedInternTable[pivot]);

            delta = Str_CompareIgnoreCase(&intern->string, Str_Text(string));
            if(delta == 0)
            {
                // Found.
                bottomIdx = topIdx+1;
                found = true;
            }
            else if(delta > 0)
            {
                if(pivot == 0)
                {
                    // Not present.
                    bottomIdx = topIdx+1;
                }
                else
                {
                    topIdx = pivot - 1;
                }
            }
            else
            {
                bottomIdx = pivot + 1;
            }
        }

        if(found && result) *result = pivot;
        return found;
    }
    return false;
}

static stringpool_intern_t* findIntern(const StringPool* pool,
    const ddstring_t* string, uint* _sortedIdx)
{
    uint sortedIdx = 0;
    boolean found = findStringSortedInternIdx(pool, string, &sortedIdx);
    if(_sortedIdx) *_sortedIdx = sortedIdx;
    if(!found) return NULL;
    return getInternByIdx(pool, pool->_stringSortedInternTable[sortedIdx]);
}

static StringPoolInternId internString(StringPool* pool, const ddstring_t* string,
    uint stringSortedMapIdx)
{
    uint idx;
    boolean isNewIntern = true; // @c true= A new intern was allocated.
    stringpool_intern_t* intern;
    StringPoolInternId id;
    assert(pool && string);

    if(((uint)-1) == pool->_numStrings)
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

    if(pool->_numStrings && stringSortedMapIdx != pool->_numStrings)
    {
        memmove(pool->_stringSortedInternTable + stringSortedMapIdx + 1,
                pool->_stringSortedInternTable + stringSortedMapIdx,
                sizeof(*pool->_stringSortedInternTable) * (pool->_numStrings - stringSortedMapIdx));
    }
    pool->_stringSortedInternTable[stringSortedMapIdx] = idx;

    // There is now one more string in the pool.
    pool->_numStrings += 1;

    return id;
}

static void removeIntern(StringPool* pool, uint sortedIdx)
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

uint StringPool_Size(const StringPool* pool)
{
    if(!pool) Con_Error("StringPool::Size: Invalid StringPool");
    return pool->_numStrings;
}

boolean StringPool_Empty(const StringPool* pool)
{
    if(!pool) Con_Error("StringPool::Emtpy: Invalid StringPool");
    return ((0 == StringPool_Size(pool))? true:false);
}

StringPoolInternId StringPool_IsInterned(const StringPool* pool, const ddstring_t* str)
{
    stringpool_intern_t* intern;
    if(!pool) Con_Error("StringPool::isInterned: Invalid StringPool");
    intern = findIntern(pool, str, NULL);
    if(!intern) return 0; // Not found.
    return internId(pool, intern);
}

const ddstring_t* StringPool_String(const StringPool* pool, StringPoolInternId internId)
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
        uint stringSortedMapIdx = 0;
        stringpool_intern_t* intern = findIntern(pool, str, &stringSortedMapIdx);

        /// \var stringSortedMapIdx is now either the index of the found intern
        /// or the left-most insertion point candidate.

        if(intern)
        {
            return internId(pool, intern);
        }

        // A new string - intern it.
        // Find the actual insertion point; scan forward.
        for(; stringSortedMapIdx < pool->_numStrings; ++stringSortedMapIdx)
        {
            const stringpool_intern_t* other = getInternByIdx(pool,
                pool->_stringSortedInternTable[stringSortedMapIdx]);

            if(Str_CompareIgnoreCase(&other->string, Str_Text(str)) > 0) break;
        }

        return internString(pool, str, stringSortedMapIdx);
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

    if(!findStringSortedInternIdx(pool, str, &sortedIdx)) return false;

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

int StringPool_Iterate(const StringPool* pool, int (*callback)(StringPoolInternId, void*),
    void* paramaters)
{
    stringpool_intern_t* intern;
    int result = 0; // Continue iteration.
    uint i;

    if(!pool) Con_Error("StringPool::Iterate: Invalid StringPool");

    intern = pool->_interns;
    for(i = 0; i < pool->_internsCount; ++i, intern++)
    {
        // Is this intern due for removable?
        if(!internUsed(pool, intern)) continue;

        result = callback(internId(pool, intern), paramaters);
        if(result) break;
    }
    return result;
}

#endif // 0

#if _DEBUG
typedef struct {
    int padding; ///< Number of characters to left-pad output.
    uint count; ///< Running total of the number of strings printed.
    const StringPool* pool; ///< StringPool instance being printed.
} printinternedstring_params_t;

static int printInternedString(StringPoolId internId, void* params)
{
    printinternedstring_params_t* p = (printinternedstring_params_t*)params;
    const ddstring_t* string = StringPool_String(p->pool, internId);
    fprintf(stderr, "%*u %5u %s\n", p->padding, p->count++, internId, Str_Text(string));
    return 0; // Continue iteration.
}

void StringPool_Print(const StringPool* pool)
{
    printinternedstring_params_t p;
    int numDigits;

    if(!pool) return;

    numDigits = 5;
    p.padding = 2 + numDigits;
    p.pool = pool;
    p.count = 0;

    fprintf(stderr, "StringPool [%p]\n    idx    id string\n", (void*)pool);
    StringPool_Iterate(pool, printInternedString, &p);
    fprintf(stderr, "  There is %u %s in the pool.\n", StringPool_Size(pool),
               StringPool_Size(pool)==1? "string":"strings");
}
#endif

#ifdef _DEBUG
LIBDENG_DEFINE_UNITTEST(StringPool)
{
    StringPool* p = StringPool_New();
    ddstring_t* s = Str_NewStd();
    ddstring_t* s2 = Str_NewStd();

    Str_Set(s, "Hello");
    assert(!StringPool_IsInterned(p, s));
    assert(StringPool_Empty(p));

    // First string.
    StringPool_Intern(p, s);
    assert(StringPool_IsInterned(p, s) == 1);

    // Re-insertion.
    assert(StringPool_Intern(p, s) == 1);

    // Case insensitivity.
    Str_Set(s, "heLLO");
    assert(StringPool_Intern(p, s) == 1);

    // Another string.
    Str_Set(s, "abc");
    const ddstring_t* is = StringPool_InternAndRetrieve(p, s);
    assert(!Str_Compare(is, Str_Text(s)));

    Str_Set(s2, "ABC");
    is = StringPool_InternAndRetrieve(p, s2);
    assert(!Str_Compare(is, Str_Text(s)));

    assert(StringPool_Intern(p, is) == 2);

    assert(StringPool_Size(p) == 2);
    StringPool_Print(p);

    assert(!StringPool_Empty(p));

    StringPool_SetUserValue(p, 1, 1234);
    assert(StringPool_UserValue(p, 1) == 1234);

    assert(StringPool_UserValue(p, 2) == 0);

    Str_Set(s, "HELLO");
    StringPool_Remove(p, s);
    assert(!StringPool_IsInterned(p, s));
    assert(StringPool_Size(p) == 1);
    assert(!Str_Compare(StringPool_String(p, 2), "abc"));

    Str_Set(s, "Third!");
    assert(StringPool_Intern(p, s) == 1);
    assert(StringPool_Size(p) == 2);

    StringPool_Clear(p);
    assert(StringPool_Empty(p));

    StringPool_Delete(p);
    Str_Delete(s);
    Str_Delete(s2);

    //exit(123);
    return true;
}
#endif

LIBDENG_RUN_UNITTEST(StringPool)
