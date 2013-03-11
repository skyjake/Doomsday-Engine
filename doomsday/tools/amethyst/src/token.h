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

#ifndef __AMETHYST_TOKEN_H__
#define __AMETHYST_TOKEN_H__

#include "string.h"
#include "shard.h"

class Source;

class Token : public Shard
{
public:
    Token(const String& tok, Source *src = 0) : Shard(Shard::TOKEN, src)
        { _token = tok; }
    String token() const { return _token; }
    void set(const String& s) { _token = s; }
    String unEscape()
    {
        if(_token == "@@") return "@";
        if(_token == "@{") return "{";
        if(_token == "@}") return "}";
        if(_token == "@$") return "$";
        return _token;
    }
    bool isIdentical(Shard *other);

protected:
    String _token;
};

#endif
