/** @file keymap.h  Key-value container with ordered keys.
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

#pragma once

#include <map>
#include "de/libcore.h"

namespace de {

/**
 * Key-value container with ordered keys (based on std::map).
 */
template <typename Key, typename Value, typename Compare = std::less<Key>>
class KeyMap : public std::map<Key, Value, Compare>
{
    using Base = std::map<Key, Value, Compare>;

public:
    KeyMap() {}

    KeyMap(const std::initializer_list<typename Base::value_type> &init)
        : Base(init)
    {}

    using iterator               = typename Base::iterator;
    using const_iterator         = typename Base::const_iterator;
    using reverse_iterator       = typename Base::reverse_iterator;
    using const_reverse_iterator = typename Base::const_reverse_iterator;

    using Base::begin;
    using Base::end;
    using Base::cbegin;
    using Base::cend;

    inline bool  isEmpty() const { return Base::empty(); }
    inline dsize size() const { return dsize(Base::size()); }
    inline int   sizei() const { return int(Base::size()); }

    iterator insert(const Key &key, const Value &value)
    {
        auto found = Base::find(key);
        if (found != Base::end())
        {
            found->second = value;
            return found;
        }
        return Base::insert(typename Base::value_type(key, value)).first;
    }

    iterator insert(const Key &key, Value &&value)
    {
        auto found = Base::find(key);
        if (found != Base::end())
        {
            found->second = std::move(value);
            return found;
        }
        return Base::insert(typename Base::value_type(key, std::move(value))).first;
    }

    void           remove(const Key &key) { Base::erase(key); }
    bool           contains(const Key &key) const { return Base::find(key) != Base::end(); }
    const_iterator constFind(const Key &key) const { return Base::find(key); }
    Value &        operator[](const Key &key) { return Base::operator[](key); }
    const Value &  operator[](const Key &key) const
    {
        auto i = Base::find(key);
        DE_ASSERT(i != Base::end());
        return i->second;
    }

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
        for (auto &i : *this) { delete i.second; }
    }
};

template <typename Key, typename Value, typename Compare = std::less<Key>>
class MutableKeyMapIterator
{
    using Container = KeyMap<Key, Value, Compare>;
    using Iterator = typename Container::iterator;

    Container _map;
    Iterator _iter;
    Iterator _cur;

public:
    MutableKeyMapIterator(Container &c) : _map(c)
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

