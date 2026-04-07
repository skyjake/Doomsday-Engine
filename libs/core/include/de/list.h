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

#include <algorithm>
#include <vector>
#include "de/libcore.h"

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
    List() = default;
    List(size_t count, const T &initValue = T()) : Base(count, initValue) {}
    List(const List &other) : Base(other) {}
    List(List &&moved) : Base(moved) {}

    template <typename Iterator>
    List(const Iterator &start, const Iterator &end)
    {
        for (Iterator i = start; i != end; ++i)
        {
            Base::push_back(i);
        }
    }

    List(const std::initializer_list<T> &init)
    {
        for (const auto &i : init)
        {
            Base::push_back(i);
        }
    }

    using iterator               = typename Base::iterator;
    using const_iterator         = typename Base::const_iterator;
    using reverse_iterator       = typename Base::reverse_iterator;
    using const_reverse_iterator = typename Base::const_reverse_iterator;

    using Base::begin;
    using Base::end;
    using Base::cbegin;
    using Base::cend;
    using Base::push_back;
    using Base::emplace_back;

    void pop_front() { removeFirst(); } // slow...
    void push_front(const T &v) { prepend(v); } // slow...

    // Qt style methods:

    inline int      count() const { return sizei(); }
    inline dsize    size() const { return dsize(Base::size()); }
    inline int      sizei() const { return int(Base::size()); }
    void            clear() { Base::clear(); }
    bool            isEmpty() const { return Base::size() == 0; }
    inline explicit operator bool() const { return !isEmpty(); }
    int             indexOf(const T &v) const
    {
        auto found = std::find(begin(), end(), v);
        if (found == end()) return -1;
        return found - begin();
    }
    bool     contains(const T &v) const { return indexOf(v) != -1; }
    void     append(const T &v) { push_back(v); }
    void     append(const List &list) { for (const T &v : list) push_back(v); }
    void     prepend(const T &v) { Base::insert(begin(), v); }
    void     insert(size_t pos, const T &value) { Base::insert(begin() + pos, value); }
    void     insert(const const_iterator &i, const T &value) { Base::insert(i, value); }
    const T &operator[](size_t pos) const { return Base::at(pos); }
    T &      operator[](size_t pos) { return Base::operator[](pos); }
    const T &at(size_t pos) const { return Base::at(pos); }
    const T &atReverse(size_t pos) const { return Base::at(size() - 1 - pos); }
    const T &first() const { return Base::front(); }
    const T &last() const { return Base::back(); }
    T &      first() { return Base::front(); }
    T &      last() { return Base::back(); }
    T        takeFirst() { T v = first(); pop_front(); return v; }
    T        takeLast()  { T v = last(); Base::pop_back(); return v; }
    T        takeAt(size_t pos) { T v = std::move(at(pos)); Base::erase(Base::begin() + pos); return v; }
    void     removeFirst() { Base::erase(Base::begin()); }
    void     removeLast()  { Base::erase(Base::begin() + size() - 1); }
    void     removeAt(size_t pos) { Base::erase(Base::begin() + pos); }
    void     removeAll(const T &v) { Base::erase(std::remove(begin(), end(), v), end()); }
    void     remove(size_t pos, size_t count) { Base::erase(cbegin() + pos, cbegin() + pos + count); }
    bool     removeOne(const T &v)
    {
        auto found = std::find(begin(), end(), v);
        if (found != end()) { Base::erase(found); return true; }
        return false;
    }
    List &   operator=(const List &other) { Base::operator=(other); return *this; }
    List &   operator=(List &&other) { Base::operator=(other); return *this; }
    void     fill(const T &value) { for (auto &i : *this) { i = value; } }

    inline List &operator<<(const T &value)
    {
        Base::push_back(value);
        return *this;
    }
    inline List &operator<<(const List &other)
    {
        for (const T &v : other) *this << v;
        return *this;
    }
    inline List &operator+=(const T &value)
    {
        Base::push_back(value);
        return *this;
    }
    inline List &operator+=(const List &other)
    {
        return *this << other;
    }
    inline List operator+(const List &other) const
    {
        List cat(*this);
        for (const T &v : other) cat << v;
        return cat;
    }
    List mid(size_t pos, size_t count = std::numeric_limits<size_t>::max()) const
    {
        auto e = count > size() - pos ? end() : (begin() + pos + count);
        return compose<List<T>>(begin() + pos, e);
    }

    void sort() { std::sort(begin(), end()); }
    void sort(const std::function<bool(const T &, const T &)> &lessThan)
    { 
        std::sort(begin(), end(), lessThan); 
    }
};

} // namespace de

#endif // LIBCORE_LIST_H
