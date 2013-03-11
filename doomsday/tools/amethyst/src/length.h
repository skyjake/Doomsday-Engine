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

#ifndef __AMETHYST_LENGTH_H__
#define __AMETHYST_LENGTH_H__

#include "string.h"

class Token;

class Length
{
public:
    enum { NotSet = -1 };
    enum ID { Invalid = -1, LeftMargin, RightMargin, Spacing, 
        Indent, NumValues };

public:
    Length();
    Length(const Length &other);

    void init(Token *token);
    void clear();
    bool isClear() const;
    bool allSet() const;
    bool has(ID id) { return inRange(id)? _values[id] != NotSet : false; }
    void set(ID id, int len) { if(inRange(id)) _values[id] = len; }
    int get(ID id) { return inRange(id)? _values[id] : 0; }
    void defaults();
    bool canLearnFrom(const Length &other);

    Length &operator += (const Length &other);

private:
    ID IDForName(const String& name);
    bool inRange(ID id) { return id >= 0 && id < NumValues; }

private:
    int _values[NumValues];
};

#endif
