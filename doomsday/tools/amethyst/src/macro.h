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

#ifndef __AMETHYST_MACRO_H__
#define __AMETHYST_MACRO_H__

#include "linkable.h"
#include "shard.h"
#include "utils.h"

class Macro : public Linkable
{
public:
    Macro(const String& n = "", Shard *macroShard = NULL, const String& args = "");

    Macro *next() { return (Macro*) Linkable::next(); }
    Macro *prev() { return (Macro*) Linkable::prev(); }

    // Getters.
    const String& name() const { return _name; }
    const String& argTypes() const { return _argTypes; }
    ArgType argType(int index);
    Shard *shard() { return &_shard; }

    Macro *find(const String& n);

private:
    String  _name;
    String  _argTypes;
    Shard   _shard;
};

#endif
