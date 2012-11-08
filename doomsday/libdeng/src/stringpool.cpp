/**
 * @file src/stringpool.cpp
 * String pool (case insensitive) implementation. @ingroup base
 *
 * @authors Copyright &copy; 2010-2012 Daniel Swanson <danij@dengine.net>
 * @authors Copyright &copy; 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/c_wrapper.h>
#include "de/memory.h"
#include "de/unittest.h"

#include <QString>
#include "de/stringpool.h"

#include <vector>
#include <list>
#include <set>
#include <algorithm>

/// Macro used for converting internal ids to externally visible Ids.
#define EXPORT_ID(i)    (uint(i) + 1)
#define IMPORT_ID(i)    (Id((i) - 1))

namespace de {

static String const nullString = "(nullptr)";

typedef uint InternalId;

/**
 * Case-insensitive text string (String).
 */
class CaselessStr
{
public:
    CaselessStr(QString text)
        : _str(text), _id(0), _userValue(0), _userPointer(0)
    {}

    CaselessStr(CaselessStr const& other)
        : _str(other._str), _id(other._id), _userValue(other._userValue), _userPointer(0)
    {}

    void setText(String& text)
    {
        _str = text;
    }
    operator String const* () const {
        return &_str;
    }
    operator String const& () const {
        return _str;
    }
    bool operator < (CaselessStr const& other) const {
        return _str.compare(other, Qt::CaseInsensitive) < 0;
    }
    bool operator == (CaselessStr const& other) const {
        return !_str.compare(other, Qt::CaseInsensitive);
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
#if 0
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
#endif

private:
    String _str;
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
    CaselessStrRef(CaselessStr const* s = 0) {
        _str = s;
    }
    CaselessStrRef(CaselessStrRef const& other) {
        _str = other._str;
    }
    CaselessStr* toStr() const {
        return const_cast<CaselessStr*>(_str);
    }
    InternalId id() const {
        DENG_ASSERT(_str);
        return _str->id();
    }
    bool operator < (CaselessStrRef const& other) const {
        DENG_ASSERT(_str);
        DENG_ASSERT(other._str);
        return *_str < *other._str;
    }
    bool operator == (CaselessStrRef const& other) const {
        DENG_ASSERT(_str);
        DENG_ASSERT(other._str);
        return *_str == *other._str;
    }
private:
    CaselessStr const* _str;
};

typedef std::set<CaselessStrRef> Interns;
typedef std::vector<CaselessStr*> IdMap;
typedef std::list<InternalId> AvailableIds;

struct StringPool::Instance
{
    /// Interned strings (owns the CaselessStr instances).
    Interns interns;

    /// InternId => CaselessStr*. Only one id can refer to the each CaselessStr*.
    IdMap idMap;

    /// Number of strings in the pool (must always be idMap.size() - available.size()).
    size_t count;

    /// List of currently unused ids in idMap.
    AvailableIds available;

    Instance() : count(0)
    {}

    ~Instance()
    {
        // Free all allocated memory.
        clear();
    }

    void clear()
    {
        for(uint i = 0; i < idMap.size(); ++i)
        {
            if(!idMap[i]) continue; // Unused slot.
            delete idMap[i];
        }
        count = 0;
        interns.clear();
        idMap.clear();
        available.clear();

        assertCount();
    }

    void inline assertCount() const
    {
        DENG_ASSERT(count == interns.size());
        DENG_ASSERT(count == idMap.size() - available.size());
    }

    Interns::iterator findIntern(String text)
    {
        CaselessStr const key(text);
        return interns.find(CaselessStrRef(&key)); // O(log n)
    }

    Interns::const_iterator findIntern(String text) const
    {
        CaselessStr const key(text);
        return interns.find(CaselessStrRef(&key)); // O(log n)
    }

    /**
     * Before this is called make sure there is no duplicate of @a text in
     * the interns set.
     *
     * @param text  Text string to add to the interned strings. A copy is
     *              made of this.
     */
    InternalId copyAndAssignUniqueId(String const& text)
    {
        CaselessStr* str = new CaselessStr(text);

        // This is a new string that is added to the pool.
        interns.insert(str); // O(log n)

        return assignUniqueId(str);
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
        DENG_ASSERT(id < idMap.size());

        CaselessStr* interned = idMap[id];
        DENG_ASSERT(interned != 0);

        idMap[id] = 0;
        available.push_back(id);

        // Delete the string itself, no one refers to it any more.
        delete interned;

        // If the caller already located the interned string, let's use it
        // to erase the string in O(1) time. Otherwise it's up to the
        // caller to make sure it gets removed from the interns.
        if(iterToErase) interns.erase(*iterToErase); // O(1) (amortized)

        // One less string.
        count--;
        assertCount();
    }
};

StringPool::StringPool()
{
    d = new Instance();
}

StringPool::StringPool(String* strings, uint count)
{
    d = new Instance();
    for(uint i = 0; strings && i < count; ++i)
    {
        intern(strings[i]);
    }
}

StringPool::~StringPool()
{
    delete d;
}

void StringPool::clear()
{
    d->clear();
}

bool StringPool::empty() const
{
    d->assertCount();
    return !d->count;
}

uint StringPool::size() const
{
    d->assertCount();
    return uint(d->count);
}

StringPool::Id StringPool::intern(String str)
{
    Interns::iterator found = d->findIntern(str); // O(log n)
    if(found != d->interns.end())
    {
        // Already got this one.
        return EXPORT_ID(found->id());
    }
    return EXPORT_ID(d->copyAndAssignUniqueId(str)); // O(log n)
}

String const& StringPool::internAndRetrieve(String str)
{
    InternalId id = IMPORT_ID(intern(str));
    return *d->idMap[id];
}

StringPool& StringPool::setUserValue(Id id, uint value)
{
    if(id == 0) return *this;

    InternalId const internalId = IMPORT_ID(id);

    DENG_ASSERT(internalId < d->idMap.size());
    DENG_ASSERT(d->idMap[internalId] != 0);

    d->idMap[internalId]->setUserValue(value); // O(1)
    return *this;
}

uint StringPool::userValue(Id id) const
{
    if(id == 0) return 0;

    InternalId const internalId = IMPORT_ID(id);

    DENG_ASSERT(internalId < d->idMap.size());
    DENG_ASSERT(d->idMap[internalId] != 0);

    return d->idMap[internalId]->userValue(); // O(1)
}

StringPool& StringPool::setUserPointer(Id id, void* ptr)
{
    if(id == 0) return *this;

    InternalId const internalId = IMPORT_ID(id);

    DENG_ASSERT(internalId < d->idMap.size());
    DENG_ASSERT(d->idMap[internalId] != 0);

    d->idMap[internalId]->setUserPointer(ptr); // O(1)
    return *this;
}

void* StringPool::userPointer(Id id) const
{
    if(id == 0) return NULL;

    InternalId const internalId = IMPORT_ID(id);

    DENG_ASSERT(internalId < d->idMap.size());
    DENG_ASSERT(d->idMap[internalId] != 0);

    return d->idMap[internalId]->userPointer(); // O(1)
}

StringPool::Id StringPool::isInterned(String str) const
{
    Interns::const_iterator found = d->findIntern(str); // O(log n)
    if(found != d->interns.end())
    {
        return EXPORT_ID(found->id());
    }
    // Not found.
    return 0;
}

String const& StringPool::string(Id id) const
{
    if(id == 0) return nullString; /// @todo Should error?

    InternalId const internalId = IMPORT_ID(id);
    DENG_ASSERT(internalId < d->idMap.size());
    return *d->idMap[internalId];
}

bool StringPool::remove(String str)
{
    Interns::iterator found = d->findIntern(str); // O(log n)
    if(found != d->interns.end())
    {
        d->releaseAndDestroy(found->id(), &found); // O(1) (amortized)
        return true;
    }
    return false;
}

bool StringPool::removeById(Id id)
{
    if(id == 0) return false;

    InternalId const internalId = IMPORT_ID(id);
    if(id >= d->idMap.size()) return false;

    CaselessStr* str = d->idMap[internalId];
    if(!str) return false;

    d->interns.erase(str); // O(log n)
    d->releaseAndDestroy(str->id());
    return true;
}

int StringPool::iterate(int (*callback)(Id, void*), void* data) const
{
    if(!callback) return 0;
    for(uint i = 0; i < d->idMap.size(); ++i)
    {
        if(!d->idMap[i]) continue;
        int result = callback(EXPORT_ID(i), data);
        if(result) return result;
    }
    return 0;
}

#if 0
void StringPool::write(Writer* writer) const
{
    // Number of strings altogether (includes unused ids).
    Writer_WriteUInt32(writer, d->idMap.size());

    // Write the interns.
    Writer_WriteUInt32(writer, d->interns.size());
    for(Interns::const_iterator i = d->interns.begin(); i != d->interns.end(); ++i)
    {
        i->toStr()->serialize(writer);
    }
}

void StringPool::read(Reader* reader)
{
    clear();

    uint numStrings = Reader_ReadUInt32(reader);
    d->idMap.resize(numStrings, 0);

    uint numInterns = Reader_ReadUInt32(reader);
    while(numInterns--)
    {
        ddstring_t text;
        Str_InitStd(&text);

        CaselessStr* str = new CaselessStr;
        str->deserialize(reader, &text);
        // Create a copy of the string whose ownership StringPool controls.
        str->setText(strdup(Str_Text(&text)));
        d->interns.insert(str);
        Str_Free(&text);

        // Update the id map.
        d->idMap[str->id()] = str;

        d->count++;
    }

    // Update the available ids.
    for(uint i = 0; i < d->idMap.size(); ++i)
    {
        if(!d->idMap[i]) d->available.push_back(i);
    }

    d->assertCount();
}
#endif

#if _DEBUG
typedef struct {
    int padding; ///< Number of characters to left-pad output.
    uint count; ///< Running total of the number of strings printed.
    StringPool const* pool; ///< StringPool instance being printed.
} printinternedstring_params_t;

static int printInternedString(StringPool::Id internId, void* params)
{
    printinternedstring_params_t* p = (printinternedstring_params_t*)params;
    QByteArray stringUtf8 = p->pool->string(internId).toUtf8();
    fprintf(stderr, "%*u %5u %s\n", p->padding, p->count++, internId, stringUtf8.constData());
    return 0; // Continue iteration.
}

void StringPool::print() const
{
    int numDigits = 5;
    printinternedstring_params_t p;
    p.padding = 2 + numDigits;
    p.pool = this;
    p.count = 0;

    fprintf(stderr, "StringPool [%p]\n    idx    id string\n", (void*)this);
    iterate(printInternedString, &p);
    fprintf(stderr, "  There is %u %s in the pool.\n", size(),
                    size() == 1? "string":"strings");
}
#endif

#ifdef _DEBUG
LIBDENG_DEFINE_UNITTEST(StringPool)
{
    StringPool p;

    String s = String("Hello");
    DENG_ASSERT(!p.isInterned(s));
    DENG_ASSERT(p.empty());

    // First string.
    p.intern(s);
    DENG_ASSERT(p.isInterned(s) == 1);

    // Re-insertion.
    DENG_ASSERT(p.intern(s) == 1);

    // Case insensitivity.
    s = String("heLLO");
    DENG_ASSERT(p.intern(s) == 1);

    // Another string.
    s = String("abc");
    String const& is = p.internAndRetrieve(s);
    DENG_ASSERT(!is.compare(s));

    String s2 = String("ABC");
    String const& is2 = p.internAndRetrieve(s2);
    DENG_ASSERT(!is2.compare(s));

    DENG_ASSERT(p.intern(is2) == 2);

    DENG_ASSERT(p.size() == 2);
    //p.print();

    DENG_ASSERT(!p.empty());

    p.setUserValue(1, 1234);
    DENG_ASSERT(p.userValue(1) == 1234);

    DENG_ASSERT(p.userValue(2) == 0);

    s = String("HELLO");
    p.remove(s);
    DENG_ASSERT(!p.isInterned(s));
    DENG_ASSERT(p.size() == 1);
    DENG_ASSERT(!p.string(2).compare("abc"));

    s = String("Third!");
    DENG_ASSERT(p.intern(s) == 1);
    DENG_ASSERT(p.size() == 2);

    s = String("FOUR");
    p.intern(s);
    p.removeById(1); // "Third!"

#if 0
    // Serialize.
    Writer* w = Writer_NewWithDynamicBuffer(0);
    p.write(w);

    // Deserialize.
    Reader* r = Reader_NewWithBuffer(Writer_Data(w), Writer_Size(w));
    StringPool p2;
    p2.read(r);
    //p2.print();
    DENG_ASSERT(p2.size() == 2);
    DENG_ASSERT(!p2.string(2).compare("abc"));
    DENG_ASSERT(!p2.string(3).compare("FOUR"));
    s = String("hello again");
    DENG_ASSERT(p2.intern(s) == 1);

    Reader_Delete(r);
    Writer_Delete(w);
#endif

    p.clear();
    DENG_ASSERT(p.empty());

    //exit(123);
    return true;
}
#endif

LIBDENG_RUN_UNITTEST(StringPool)

} // namespace de
