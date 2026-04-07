/** @file stringlist.h  List of strings.
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

#ifndef LIBCORE_STRINGLIST_H
#define LIBCORE_STRINGLIST_H

#include "de/string.h"

namespace de {

/**
 * List of strings.
 */
class StringList
{
public:
    StringList();

    template <typename T>
    StringList(const std::initializer_list<T> &init) {
        for (const auto &v : init) {
            append(v);
        }
    }

    inline bool isEmpty() const { return size() == 0; }
    inline bool empty() const { return isEmpty(); }

    void append(const String &);
    void prepend(const String &);
    void insert(int pos, const String &);

    const String &first() const;
    const String &back() const;

    const String &front() const;
    const String &back() const;
    inline void push_back(const String &s) { append(s); }
    inline void pop_back() { removeLast(); }

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBCORE_STRINGLIST_H
