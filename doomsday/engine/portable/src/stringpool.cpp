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

#include "de_platform.h"
#include <de/memoryzone.h>
#include <de/str.hh>
#include "stringpool.h"
#include "unittest.h"

#include <stdio.h>
#include <vector>
#include <list>
#include <set>
#include <algorithm>

/**
 * @def LIBDENG_STRINGPOOL_ZONE_ALLOCS
 * Define this to make StringPool allocate memory from the memory zone instead
 * of with system malloc().
 */
//#define LIBDENG_STRINGPOOL_ZONE_ALLOCS

#ifdef LIBDENG_STRINGPOOL_ZONE_ALLOCS
#  define STRINGPOOL_STRDUP(x)  Z_StrDup(x)
#  define STRINGPOOL_FREE(x)    Z_Free(x)
#else
#  define STRINGPOOL_STRDUP(x)  strdup(x)
#  define STRINGPOOL_FREE(x)    free(x)
#endif

/// Macro used for converting internal ids to externally visible StringPoolIds.
#define EXPORT_ID(i)    (uint(i) + 1)
#define IMPORT_ID(i)    (StringPoolId((i) - 1))

static const de::Str nullString = "(nullptr)";

typedef uint InternalId;

/**
 * Case-insensitive reference to a text string (ddstring).
 * Does not take copies or retain ownership of the string.
 */
class CaselessStr
{
public:
    CaselessStr(const char* text = 0) : _id(0), _userValue(0), _userPointer(0) {
        setText(text);
    }
    CaselessStr(const CaselessStr& other)
        : _id(other._id), _userValue(other._userValue), _userPointer(0) {
        setText(other._str.str);
    }
    void setText(const char* text) {
        Str_InitStatic(&_str, text); // no ownership taken; not copied
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
    void* userPointer() const {
        return _userPointer;
    }
    void setUserPointer(void* ptr) {
        _userPointer = ptr;
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
    void* _userPointer;
};

/**
 * Utility class that acts as the value type for the std::set of interned
 * strings. Only points to CaselessStr instances. The less-than operator is
 * needed by std::set to keep the strings ordered.
 */
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
            str->setText(STRINGPOOL_STRDUP(Str_Text(&text)));
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
        CaselessStr* str = new CaselessStr(STRINGPOOL_STRDUP(text));

        // This is a new string that is added to the pool.
        interns.insert(str); // O(log n)

        return assignUniqueId(str);
    }

    void destroyStr(CaselessStr* str)
    {
        STRINGPOOL_FREE(str->toCString()); // duplicated cstring
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
    const InternalId internalId = IMPORT_ID(id);

    assert(pool);
    assert(internalId < pool->idMap.size());
    assert(pool->idMap[internalId] != 0);

    pool->idMap[internalId]->setUserValue(value); // O(1)
}

uint StringPool_UserValue(StringPool* pool, StringPoolId id)
{
    const InternalId internalId = IMPORT_ID(id);

    assert(pool);
    assert(internalId < pool->idMap.size());
    assert(pool->idMap[internalId] != 0);

    return pool->idMap[internalId]->userValue(); // O(1)
}

void StringPool_SetUserPointer(StringPool* pool, StringPoolId id, void* ptr)
{
    const InternalId internalId = IMPORT_ID(id);

    assert(pool);
    assert(internalId < pool->idMap.size());
    assert(pool->idMap[internalId] != 0);

    pool->idMap[internalId]->setUserPointer(ptr); // O(1)
}

void* StringPool_UserPointer(StringPool* pool, StringPoolId id)
{
    const InternalId internalId = IMPORT_ID(id);

    assert(pool);
    assert(internalId < pool->idMap.size());
    assert(pool->idMap[internalId] != 0);

    return pool->idMap[internalId]->userPointer(); // O(1)
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
#ifdef LIBDENG_STRINGPOOL_ZONE_ALLOCS
    return 0; // Zone is not available yet.
#endif

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
    //StringPool_Print(p);

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

    Str_Set(s, "FOUR");
    StringPool_Intern(p, s);
    StringPool_RemoveById(p, 1); // "Third!"

    // Serialize.
    Writer* w = Writer_NewWithDynamicBuffer(0);
    StringPool_Write(p, w);

    // Deserialize.
    Reader* r = Reader_NewWithBuffer(Writer_Data(w), Writer_Size(w));
    StringPool* p2 = StringPool_New();
    StringPool_Read(p2, r);
    //StringPool_Print(p2);
    assert(StringPool_Size(p2) == 2);
    assert(!Str_Compare(StringPool_String(p2, 2), "abc"));
    assert(!Str_Compare(StringPool_String(p2, 3), "FOUR"));
    Str_Set(s, "hello again");
    assert(StringPool_Intern(p2, s) == 1);

    StringPool_Delete(p2);
    Reader_Delete(r);
    Writer_Delete(w);

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
