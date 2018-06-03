/** @file
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

#ifndef LIBCORE_LIST_H
#define LIBCORE_LIST_H

#include <vector>

namespace de {

/**
 * Array of elements.
 * @ingroup data
 */
template <typename T>
class List : public std::vector<T>
{
    using Base = std::vector<T>;

public:
    List() {}
    List(const List &);
    List(List &&);
    List(const std::initializer_list<T> &init) {
        for (const auto &i : init) {
            push_back(i);
        }
    }

    using iterator               = typename Base::iterator;
    using const_iterator         = typename Base::const_iterator;
    using reverse_iterator       = typename Base::reverse_iterator;
    using const_reverse_iterator = typename Base::const_reverse_iterator;

    // Qt style methods:

    int      size() const { return int(Base::size()); }
    void     clear() { Base::clear(); }
    bool     isEmpty() const { return Base::size() == 0; }
    void     append(const T &s) { Base::push_back(s); }
    void     prepend(const T &s) { Base::push_front(s); }
    void     insert(int pos, const T &value) { Base::insert(Base::begin() + pos, value); }
    const T &operator[](int pos) const { return Base::at(pos); }
    T &      operator[](int pos) { return Base::operator[](pos); }
    const T &at(int pos) const { return Base::at(pos); }
    const T &first() const { return Base::front(); }
    const T &last() const { return Base::back(); }
    T        takeFirst() { T v = first(); Base::pop_front(); return v; }
    T        takeLast()  { T v = last();  Base::pop_back();  return v; }
    void     removeFirst() { Base::erase(Base::begin()); }
    void     removeLast()  { Base::erase(Base::begin() + size() - 1); }
    void     removeAt(int pos) { Base::erase(Base::begin() + pos); }
};

} // namespace de

#endif // LIBCORE_LIST_H
