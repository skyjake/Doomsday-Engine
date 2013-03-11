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

#include "rule.h"

Rule::Rule() : Linkable()
{
    _type = GENERIC;
}

/**
 * Returns true if one of the terms matches the gem.
 */
bool Rule::matches(Gem *gem)
{
    if(_terms.isListEmpty()) return true; // Matches everything.
    // Try all the terms, accept the first match.
    for(GemTest *t = _terms.next(); !t->isRoot(); t = t->next())
        if(t->test(gem)) return true;
    return false;
}
