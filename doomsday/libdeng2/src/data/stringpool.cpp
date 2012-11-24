/**
 * @file stringpool.cpp
 * Pool of strings (case insensitive).
 *
 * @author Copyright &copy; 2010-2012 Daniel Swanson <danij@dengine.net>
 * @author Copyright &copy; 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/StringPool"
#include "de/Reader"
#include "de/Writer"

#include <vector>
#include <list>
#include <set>
#include <algorithm>
#ifdef _DEBUG
#  include <stdio.h> /// @todo should use C++
#endif

/// Macro used for converting internal ids to externally visible Ids.
#define EXPORT_ID(i)    (uint(i) + 1)
#define IMPORT_ID(i)    (Id((i) - 1))

namespace de {

static String const nullString = "(nullptr)";

typedef uint InternalId;

/**
 * Case-insensitive text string (String).
 */
class CaselessString : public ISerializable
{
public:
    CaselessString()
        : _str(), _id(0), _userValue(0), _userPointer(0)
    {}

    CaselessString(QString text)
        : _str(text), _id(0), _userValue(0), _userPointer(0)
    {}

    CaselessString(CaselessString const &other)
        : ISerializable(), _str(other._str), _id(other._id), _userValue(other._userValue), _userPointer(0)
    {}

    void setText(String &text)
    {
        _str = text;
    }
    operator String const *() const {
        return &_str;
    }
    operator String const &() const {
        return _str;
    }
    bool operator < (CaselessString const &other) const {
        return _str.compare(other, Qt::CaseInsensitive) < 0;
    }
    bool operator == (CaselessString const &other) const {
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
    void *userPointer() const {
        return _userPointer;
    }
    void setUserPointer(void *ptr) {
        _userPointer = ptr;
    }
    // Implements ISerializable.
    void operator >> (Writer &to) const {
        to << _str << duint32(_id) << duint32(_userValue);
    }
    void operator << (Reader &from) {
        from >> _str >> _id >> _userValue;
    }

private:
    String _str;
    InternalId _id; ///< The id that refers to this string.
    uint _userValue;
    void *_userPointer;
};

/**
 * Utility class that acts as the value type for the std::set of interned
 * strings. Only points to CaselessString instances. The less-than operator
 * is needed by std::set to keep the strings ordered.
 */
class CaselessStringRef {
public:
    CaselessStringRef(CaselessString const *s = 0) {
        _str = s;
    }
    CaselessStringRef(CaselessStringRef const &other) {
        _str = other._str;
    }
    CaselessString *toStr() const {
        return const_cast<CaselessString *>(_str);
    }
    InternalId id() const {
        DENG2_ASSERT(_str);
        return _str->id();
    }
    bool operator < (CaselessStringRef const &other) const {
        DENG2_ASSERT(_str);
        DENG2_ASSERT(other._str);
        return *_str < *other._str;
    }
    bool operator == (CaselessStringRef const &other) const {
        DENG2_ASSERT(_str);
        DENG2_ASSERT(other._str);
        return *_str == *other._str;
    }
private:
    CaselessString const *_str;
};

typedef std::set<CaselessStringRef> Interns;
typedef std::vector<CaselessString *> IdMap;
typedef std::list<InternalId> AvailableIds;

struct StringPool::Instance
{
    /// Interned strings (owns the CaselessString instances).
    Interns interns;

    /// InternId => CaselessString*. Only one id can refer to the each CaselessString*.
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
        DENG2_ASSERT(count == interns.size());
        DENG2_ASSERT(count == idMap.size() - available.size());
    }

    Interns::iterator findIntern(String text)
    {
        CaselessString const key(text);
        return interns.find(CaselessStringRef(&key)); // O(log n)
    }

    Interns::const_iterator findIntern(String text) const
    {
        CaselessString const key(text);
        return interns.find(CaselessStringRef(&key)); // O(log n)
    }

    /**
     * Before this is called make sure there is no duplicate of @a text in
     * the interns set.
     *
     * @param text  Text string to add to the interned strings. A copy is
     *              made of this.
     */
    InternalId copyAndAssignUniqueId(String const &text)
    {
        CaselessString *str = new CaselessString(text);

        // This is a new string that is added to the pool.
        interns.insert(str); // O(log n)

        return assignUniqueId(str);
    }

    InternalId assignUniqueId(CaselessString *str) // O(1)
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

    void releaseAndDestroy(InternalId id, Interns::iterator *iterToErase = 0)
    {
        DENG2_ASSERT(id < idMap.size());

        CaselessString *interned = idMap[id];
        DENG2_ASSERT(interned != 0);

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

StringPool::StringPool(String *strings, uint count)
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

String StringPool::internAndRetrieve(String str)
{
    InternalId id = IMPORT_ID(intern(str));
    return *d->idMap[id];
}

void StringPool::setUserValue(Id id, uint value)
{
    if(id == 0) return;

    InternalId const internalId = IMPORT_ID(id);

    DENG2_ASSERT(internalId < d->idMap.size());
    DENG2_ASSERT(d->idMap[internalId] != 0);

    d->idMap[internalId]->setUserValue(value); // O(1)
}

uint StringPool::userValue(Id id) const
{
    if(id == 0) return 0;

    InternalId const internalId = IMPORT_ID(id);

    DENG2_ASSERT(internalId < d->idMap.size());
    DENG2_ASSERT(d->idMap[internalId] != 0);

    return d->idMap[internalId]->userValue(); // O(1)
}

void StringPool::setUserPointer(Id id, void *ptr)
{
    if(id == 0) return;

    InternalId const internalId = IMPORT_ID(id);

    DENG2_ASSERT(internalId < d->idMap.size());
    DENG2_ASSERT(d->idMap[internalId] != 0);

    d->idMap[internalId]->setUserPointer(ptr); // O(1)
}

void *StringPool::userPointer(Id id) const
{
    if(id == 0) return NULL;

    InternalId const internalId = IMPORT_ID(id);

    DENG2_ASSERT(internalId < d->idMap.size());
    DENG2_ASSERT(d->idMap[internalId] != 0);

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

String StringPool::string(Id id) const
{
    /// @throws InvalidIdError Provided identifier is not in use.
    return stringRef(id);
}

String const &StringPool::stringRef(StringPool::Id id) const
{
    if(id == 0)
    {
        /// @throws InvalidIdError Provided identifier is not in use.
        //throw InvalidIdError("StringPool::stringRef", "Invalid identifier");
        static String emptyString;
        return emptyString;
    }

    InternalId const internalId = IMPORT_ID(id);
    DENG2_ASSERT(internalId < d->idMap.size());
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

    CaselessString *str = d->idMap[internalId];
    if(!str) return false;

    d->interns.erase(str); // O(log n)
    d->releaseAndDestroy(str->id());
    return true;
}

int StringPool::iterate(int (*callback)(Id, void *), void *data) const
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

// Implements ISerializable.
void StringPool::operator >> (Writer &to) const
{
    // Number of strings altogether (includes unused ids).
    to << duint32(d->idMap.size());

    // Write the interns.
    to << duint32(d->interns.size());
    for(Interns::const_iterator i = d->interns.begin(); i != d->interns.end(); ++i)
    {
        to << *i->toStr();
    }
}

void StringPool::operator << (Reader &from)
{
    clear();

    // Read the number of total number of strings.
    uint numStrings;
    from >> numStrings;
    d->idMap.resize(numStrings, 0);

    // Read the interns.
    uint numInterns;
    from >> numInterns;
    while(numInterns--)
    {
        CaselessString *str = new CaselessString;
        from >> *str;
        d->interns.insert(str);

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

#if _DEBUG
typedef struct {
    int padding; ///< Number of characters to left-pad output.
    uint count; ///< Running total of the number of strings printed.
    StringPool const *pool; ///< StringPool instance being printed.
} printinternedstring_params_t;

static int printInternedString(StringPool::Id internId, void *params)
{
    printinternedstring_params_t *p = (printinternedstring_params_t *)params;
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

    fprintf(stderr, "StringPool [%p]\n    idx    id string\n", (void *)this);
    iterate(printInternedString, &p);
    fprintf(stderr, "  There is %u %s in the pool.\n", size(),
                    size() == 1? "string":"strings");
}
#endif

} // namespace de
