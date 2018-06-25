/** @file map.h  Key-value container with ordered keys.
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

#ifndef LIBCORE_MAP_H
#define LIBCORE_MAP_H

#include <map>

namespace de {

/**
 * Key-value container with ordered keys (based on std::map).
 */
template <typename Key, typename Value, typename Compare = std::less<Key>>
class Map : public std::map<Key, Value, Compare>
{
    using Base = std::map<Key, Value, Compare>;

public:
    Map() {}

    using iterator               = typename Base::iterator;
    using const_iterator         = typename Base::const_iterator;
    using reverse_iterator       = typename Base::reverse_iterator;
    using const_reverse_iterator = typename Base::const_reverse_iterator;

    void insert(const Key &key, const Value &value) { Base::operator[](key) = value; }
    void remove(const Key &key) { Base::erase(key); }
    bool contains(const Key &key) const { return Base::find(key) != Base::end(); }
    const_iterator constFind(const Key &key) const { return Base::find(key); }

    inline Value take(const Key &key)
    {
        DE_ASSERT(contains(key));
        auto  found = Base::find(key);
        Value v     = found->second;
        Base::erase(found);
        return v;
    }

    void deleteAll()
    {
        for (const auto &i : *this) { delete i.second; }
    }
};

template <typename Key, typename Value, typename Compare = std::less<Key>>
class MutableMapIterator
{
    using Container = Map<Key, Value, Compare>;
    using Iterator = typename Container::iterator;

    Container _map;
    Iterator _iter;
    Iterator _cur;

public:
    MutableMapIterator(Container &c) : _map(c)
    {
        _iter = _map.begin();
    }

    bool hasNext() const
    {
        return _iter != _map.end();
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
        _iter = _map.erase(_cur);
    }
};

} // namespace de

#endif // LIBCORE_MAP_H
