/** @file stringpool.cpp  Pool of strings (case insensitive).
 *
 * @author Copyright © 2010-2015 Daniel Swanson <danij@dengine.net>
 * @author Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/stringpool.h"
#include "de/reader.h"
#include "de/writer.h"
#include "de/lockable.h"
#include "de/guard.h"

#include <vector>
#include <list>
#include <set>
#include <algorithm>
#ifdef DE_DEBUG
#  include <iostream>
#  include <iomanip>
#endif

/// Macro used for converting internal ids to externally visible Ids.
#define EXPORT_ID(i)    (duint32(i) + 1)
#define IMPORT_ID(i)    (Id((i) - 1))

#define MAXIMUM_VALID_ID (0xffffffff - 1)

namespace de {

typedef uint InternalId;

#if 0
/**
 * Case-insensitive text string (String).
 */
class CaselessString : public ISerializable
{
public:
    CaselessString()
        : _str(), _id(0), _userValue(0), _userPointer(0)
    {}

    CaselessString(const String &text)
        : _str(text), _id(0), _userValue(0), _userPointer(0)
    {}

    CaselessString(const CaselessString &other)
        : _str(other._str), _id(other._id), _userValue(other._userValue), _userPointer(0)
    {}

    void setText(const String &text)
    {
        _str = text;
    }
    operator const String *() const {
        return &_str;
    }
    operator const String &() const {
        return _str;
    }
    bool operator < (const CaselessString &other) const {
        return _str.compareWithoutCase(other) < 0;
    }
    bool operator == (const CaselessString &other) const {
        return _str.compareWithoutCase(other) == 0;
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
    CaselessStringRef(const CaselessString *s = 0) {
        _str = s;
    }
    CaselessStringRef(const CaselessStringRef &other) {
        _str = other._str;
    }
    CaselessString *toStr() const {
        return const_cast<CaselessString *>(_str);
    }
    InternalId id() const {
        DE_ASSERT(_str);
        return _str->id();
    }
    bool operator < (const CaselessStringRef &other) const {
        DE_ASSERT(_str);
        DE_ASSERT(other._str);
        return *_str < *other._str;
    }
    bool operator == (const CaselessStringRef &other) const {
        DE_ASSERT(_str);
        DE_ASSERT(other._str);
        return *_str == *other._str;
    }
private:
    const CaselessString *_str;
};
#endif

struct Intern : public ISerializable
{
    String str;
    InternalId id;
    uint userValue = 0;
    void *userPointer = nullptr;

    Intern(const String &s, InternalId intId = 0) : str(s), id(intId) {}
    inline bool operator<(const Intern &other) const
    {
        return str.compareWithoutCase(other.str) < 0;
    }
    struct PtrLess
    {
        bool operator()(const Intern *a, const Intern *b) const
        {
            return *a < *b;
        }
    };
    void operator >> (Writer &to) const
    {
        to << str << duint32(id) << duint32(userValue);
    }
    void operator << (Reader &from)
    {
        from >> str >> id >> userValue;
    }
};
typedef std::set<Intern *, Intern::PtrLess> Interns; // owned
typedef std::vector<Intern *> IdMap; // points to interns
typedef std::list<InternalId> AvailableIds;

DE_PIMPL_NOREF(StringPool), public Lockable
{
    /// Interned strings.
    Interns interns;

    /// ID-based lookup table.
    IdMap idMap;

    /// Number of strings in the pool (must always be idMap.size() - available.size()).
    dsize count = 0;

    /// List of currently unused ids in idMap.
    AvailableIds available;

    ~Impl()
    {
        // Free all allocated memory.
        clear();
    }

    void clear()
    {
        DE_GUARD(this);

        deleteAll(interns);
        interns.clear();
        count = 0;
        idMap.clear();
        available.clear();

        assertCount();
    }

    inline void assertCount() const
    {
        DE_ASSERT(count == interns.size());
        DE_ASSERT(count == idMap.size() - available.size());
    }

    Interns::iterator findIntern(const String &text)
    {
        Intern key(text);
        return interns.find(&key); // O(log n)
    }

    Interns::const_iterator findIntern(const String &text) const
    {
        Intern key(text);
        return interns.find(&key); // O(log n)
    }

    /**
     * Before this is called make sure there is no duplicate of @a text in
     * the interns set.
     *
     * @param text  Text string to add to the interned strings. A copy is
     *              made of this.
     */
    InternalId copyAndAssignUniqueId(const String &text)
    {
        // This is a new string that is added to the pool.
        auto *intern = new Intern(text.lower(), nextId());
        interns.insert(intern); // O(log n)
        idMap[intern->id] = intern;
        assertCount();
        return intern->id;
    }

    InternalId nextId() // O(1)
    {
        InternalId idx;

        // Any available ids in the shortlist?
        if (!available.empty()) // O(1)
        {
            idx = available.front();
            available.pop_front();
        }
        else
        {
            if (idMap.size() >= MAXIMUM_VALID_ID)
            {
                throw StringPool::FullError("StringPool::nextId",
                                            "Out of valid 32-bit identifiers");
            }
            // Expand the idMap.
            idx = InternalId(idMap.size());
            idMap.push_back(nullptr);
        }

        // We have one more string in the pool.
        count++;

        return idx;
    }

    void releaseAndDestroy(InternalId id, Interns::iterator *iterToErase = nullptr)
    {
        DE_ASSERT(id < idMap.size());

        const Intern *interned = idMap[id];
        DE_ASSERT(interned);

        idMap[id] = nullptr;
        available.push_back(id);

        // Delete the string itself, no one refers to it any more.
        delete interned;

        // If the caller already located the interned string, let's use it
        // to erase the string in O(1) time. Otherwise it's up to the
        // caller to make sure it gets removed from the interns.
        if (iterToErase) interns.erase(*iterToErase); // O(1) (amortized)

        // One less string.
        count--;
        assertCount();
    }
};

StringPool::StringPool() : d(new Impl)
{}

StringPool::StringPool(const String *strings, uint count) : d(new Impl)
{
    DE_GUARD(d);
    for (uint i = 0; strings && i < count; ++i)
    {
        intern(strings[i]);
    }
}

void StringPool::clear()
{
    d->clear();
}

bool StringPool::empty() const
{
    DE_GUARD(d);
    d->assertCount();
    return !d->count;
}

dsize StringPool::size() const
{
    DE_GUARD(d);
    d->assertCount();
    return d->count;
}

StringPool::Id StringPool::intern(const String &str)
{
    DE_GUARD(d);
    auto found = d->findIntern(str); // O(log n)
    if (found != d->interns.end())
    {
        // Already got this one.
        return EXPORT_ID((*found)->id);
    }
    return EXPORT_ID(d->copyAndAssignUniqueId(str)); // O(log n)
}

String StringPool::internAndRetrieve(const String &str)
{
    DE_GUARD(d);
    InternalId id = IMPORT_ID(intern(str));
    return d->idMap[id]->str;
}

void StringPool::setUserValue(Id id, uint value)
{
    if (id == 0) return;

    const InternalId internalId = IMPORT_ID(id);

    DE_GUARD(d);

    DE_ASSERT(internalId < d->idMap.size());
    DE_ASSERT(d->idMap[internalId] != nullptr);

    d->idMap[internalId]->userValue = value;
}

uint StringPool::userValue(Id id) const
{
    if (id == 0) return 0;

    const InternalId internalId = IMPORT_ID(id);

    DE_GUARD(d);

    DE_ASSERT(internalId < d->idMap.size());
    DE_ASSERT(d->idMap[internalId] != 0);

    return d->idMap[internalId]->userValue;
}

void StringPool::setUserPointer(Id id, void *ptr)
{
    if (id == 0) return;

    const InternalId internalId = IMPORT_ID(id);

    DE_GUARD(d);

    DE_ASSERT(internalId < d->idMap.size());
    DE_ASSERT(d->idMap[internalId] != nullptr);

    d->idMap[internalId]->userPointer = ptr;
}

void *StringPool::userPointer(Id id) const
{
    if (id == 0) return nullptr;

    const InternalId internalId = IMPORT_ID(id);

    DE_GUARD(d);

    DE_ASSERT(internalId < d->idMap.size());
    DE_ASSERT(d->idMap[internalId] != nullptr);

    return d->idMap[internalId]->userPointer;
}

StringPool::Id StringPool::isInterned(const String &str) const
{
    DE_GUARD(d);

    Interns::const_iterator found = d->findIntern(str); // O(log n)
    if (found != d->interns.end())
    {
        return EXPORT_ID((*found)->id);
    }
    // Not found.
    return 0;
}

String StringPool::string(Id id) const
{
    DE_GUARD(d);

    /// @throws InvalidIdError Provided identifier is not in use.
    return stringRef(id);
}

const String &StringPool::stringRef(StringPool::Id id) const
{
    if (id == 0)
    {
        /// @throws InvalidIdError Provided identifier is not in use.
        //throw InvalidIdError("StringPool::stringRef", "Invalid identifier");
        static String emptyString;
        return emptyString;
    }

    DE_GUARD(d);

    const InternalId internalId = IMPORT_ID(id);
    DE_ASSERT(internalId < d->idMap.size());
    return d->idMap[internalId]->str;
}

bool StringPool::remove(const String &str)
{
    DE_GUARD(d);

    Interns::iterator found = d->findIntern(str); // O(log n)
    if (found != d->interns.end())
    {
        d->releaseAndDestroy((*found)->id, &found); // O(1) (amortized)
        return true;
    }
    return false;
}

bool StringPool::removeById(Id id)
{
    if (id == 0) return false;

    DE_GUARD(d);

    const InternalId internalId = IMPORT_ID(id);
    if (id >= d->idMap.size()) return false;

    auto *intern = d->idMap[internalId];
    if (!intern) return false;

    d->interns.erase(intern); // O(log n)
    d->releaseAndDestroy(intern->id);
    return true;
}

LoopResult StringPool::forAll(const std::function<LoopResult (Id)>& func) const
{
    DE_GUARD(d);
    for (duint i = 0; i < d->idMap.size(); ++i)
    {
        if (d->idMap[i])
        {
            if (auto result = func(EXPORT_ID(i)))
                return result;
        }
    }
    return LoopContinue;
}

// Implements ISerializable.
void StringPool::operator>>(Writer &to) const
{
    DE_GUARD(d);

    // Number of strings altogether (includes unused ids).
    to << duint32(d->idMap.size());

    // Write the interns.
    to << duint32(d->interns.size());
    for (Interns::const_iterator i = d->interns.begin(); i != d->interns.end(); ++i)
    {
        to << **i;
    }
}

void StringPool::operator<<(Reader &from)
{
    DE_GUARD(d);

    clear();

    // Read the number of total number of strings.
    uint numStrings;
    from >> numStrings;
    d->idMap.resize(numStrings, nullptr);

    // Read the interns.
    uint numInterns;
    from >> numInterns;
    while (numInterns--)
    {
        Intern *intern = new Intern(String());
        from >> *intern;
        d->interns.insert(intern);

        // Update the id map.
        d->idMap[intern->id] = intern;

        d->count++;
    }

    // Update the available ids.
    for (uint i = 0; i < d->idMap.size(); ++i)
    {
        if (!d->idMap[i]) d->available.push_back(i);
    }

    d->assertCount();
}

#ifdef DE_DEBUG
void StringPool::print() const
{
    using namespace std;
    
    cerr << "StringPool [" << this << "]" << endl << "    idx    id string" << endl;
    duint count = 0;
    forAll([&count](Id id) {
        cerr << setw(5) << count++ << " " << setw(5) << id << " " << id << endl;
        return LoopContinue;
    });
    cerr << "  There are " << size() << " string" << DE_PLURAL_S(size()) << " in the pool." << endl;
}
#endif

}  // namespace de
