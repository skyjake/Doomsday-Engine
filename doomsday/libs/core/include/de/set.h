/** @file set.h  Container with unordered values.
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

#ifndef LIBCORE_SET_H
#define LIBCORE_SET_H

#include <unordered_set>
#include <ostream>

namespace de {

/**
 * Container with unordered values (based on std::unordered_set).
 * @ingroup data
 */
template <typename Value>
class Set : public std::unordered_set<Value>
{
    using Base = std::unordered_set<Value>;

public:
    Set() {}
    Set(const Set &copied) : Base(copied) {}
    Set(Set &&moved) : Base(moved) {}

    template <typename T>
    Set(const std::initializer_list<T> &init)
    {
        for (const auto &i : init) insert(i);
    }

    using Base::operator=;
    using Base::begin;
    using Base::end;
    using Base::insert;

    // Qt style methods:

    int      size() const { return int(Base::size()); }
    bool     isEmpty() const { return Base::empty(); }
    void     clear() { Base::clear(); }
    void     remove(const Value &value) { Base::erase(value); }
    bool     contains(const Value &value) const { return Base::find(value) != end(); }
    Set &    operator<<(const Value &value) { Base::insert(value); return *this; }
    Set &    operator=(const Set &&copied) { Base::operator=(copied); return *this; }
    Set &    operator=(Set &&moved) { Base::operator=(moved); return *this; }

    inline Set &push_back(const Value &value) { return *this << value; }
};

template <typename T>
inline std::ostream &operator<<(std::ostream &os, const Set<T> &set)
{
    os << "Set{";
    for (const auto &v : set)
    {
        os << " " << v;
    }
    os << " }";
    return os;
}

} // namespace de

#endif // LIBCORE_SET_H
