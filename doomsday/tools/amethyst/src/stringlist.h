/*
 * Copyright (c) 2002-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __AMETHYST_STRING_LIST_H__
#define __AMETHYST_STRING_LIST_H__

#include "string.h"
#include "linkable.h"

class StringList : public Linkable
{
public:
    StringList() : Linkable(true) {}
    StringList(const String& s) : Linkable() { _str = s; }

    StringList *next() { return (StringList*) Linkable::next(); }
    StringList *prev() { return (StringList*) Linkable::prev(); }

    const String& get() const { return _str; }
    String& get() { return _str; }
    void set(const String& s) { _str = s; }

protected:
    String  _str;
};

#endif
