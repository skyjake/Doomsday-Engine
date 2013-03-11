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

#include "macro.h"

Macro::Macro(const String& n, Shard *macroShard, const String& args)
: Linkable()
{
    _name = n;
    _argTypes = args;
    if(macroShard) 
    {
        // Steal macroShard's children.
        Shard *next;
        for(Shard *it = macroShard->first(); it; it = next)
        {
            next = it->next();
            _shard.add(macroShard->remove(it));
        }
    }
}

ArgType Macro::argType(int index)
{
    return interpretArgType(_argTypes, index);
}

Macro *Macro::find(const String& n)
{
    for(Macro *it = next(); !it->isRoot(); it = it->next())
        if(it->_name == n) return it;
    return NULL;
}
