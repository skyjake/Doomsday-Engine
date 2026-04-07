/** @file hash.h  Efficient key-value container with unordered keys.
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_HASH_H
#define LIBCORE_HASH_H

#include <unordered_map>
#include <list>
#include "de/libcore.h"

namespace de {

/**
 * Efficient key-value container with unordered keys (based on std::unordered_map).
 * @ingroup data
 */
template <typename Key,
          typename Value,
          typename HashFn = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
class Hash : public std::unordered_map<Key, Value, HashFn, KeyEqual>
{
    using Base = std::unordered_map<Key, Value, HashFn, KeyEqual>;

public:
    Hash() {}
    Hash(const Hash &copied) : Base(copied) {}
    Hash(Hash &&moved) : Base(moved) {}

    Hash(const std::initializer_list<typename Base::value_type> &init)
    {
        for (const auto &v : init) Base::insert(v);
    }

    using iterator       = typename Base::iterator;
    using const_iterator = typename Base::const_iterator;
    using value_type     = typename Base::value_type;

    using Base::begin;
    using Base::empty;
    using Base::end;
    using Base::find;

    bool isEmpty() const { return empty(); }
    inline dsize size() const { return dsize(Base::size()); }
    inline int sizei() const { return int(Base::size()); }

    iterator insert(const Key &key, const Value &value)
    {
        auto found = find(key);
        if (found != end())
        {
            found->second = value;
            return found;
        }
        return Base::insert(std::make_pair(key, value)).first;
    }
    iterator insert(const Key &key, Value &&value)
    {
        auto found = find(key);
        if (found != end())
        {
            found->second = value;
            return found;
        }
        return Base::insert(std::make_pair(key, value)).first;
    }
    void         remove(const Key &key) { Base::erase(key); }
    bool         contains(const Key &key) const { return Base::find(key) != Base::end(); }
    Value &      operator[](const Key &key) { return Base::operator[](key); }
    const Value &operator[](const Key &key) const { return Base::find(key)->second; }
    Hash &       operator=(const Hash &copied) { Base::operator=(copied); return *this; }
    Hash &       operator=(Hash &&moved) { Base::operator=(moved); return *this; }

    Value take(const Key &key)
    {
        auto  found = find(key);
        Value v     = std::move(found->second);
        Base::erase(found);
        return v;
    }

    void deleteAll()
    {
        for (auto &i : *this) { delete i.second; }
    }

    std::list<Key> keys() const
    {
        return map<std::list<Key>>(*this, [](const value_type &v) {
            return v.first;
        });
    }
};

template <typename Key,
          typename Value,
          typename HashFn = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
class MutableHashIterator
{
    using Container = Hash<Key, Value, HashFn, KeyEqual>;
    using Iterator = typename Container::iterator;

    Container _hash;
    Iterator _iter;
    Iterator _cur;

public:
    MutableHashIterator(Container &c) : _hash(c)
    {
        _iter = _hash.begin();
    }

    bool hasNext() const
    {
        return _iter != _hash.end();
    }

    Iterator &next()
    {
        _cur = _iter++;
        return _cur;
    }

    const typename Container::key_type &key() const
    {
        return _cur->first;
    }

    const typename Container::value_type::second_type &value() const
    {
        return _cur->second;
    }

    void remove()
    {
        _iter = _hash.erase(_cur);
    }
};

template <typename MultiContainer>
inline bool multiRemove(MultiContainer &multi,
                        const typename MultiContainer::value_type::first_type &key,
                        const typename MultiContainer::value_type::second_type &value)
{
    bool removed = false;
    const auto keys = multi.equal_range(key);
    for (auto i = keys.first; i != keys.second; )
    {
        if (i->second == value)
        {
            i = multi.erase(i);
            removed = true;
        }
        else
        {
            ++i;
        }
    }
    return removed;
}

} // namespace de

#endif // LIBCORE_HASH_H
