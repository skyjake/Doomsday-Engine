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

#include "commandrule.h"

CommandRule::CommandRule() : Linkable()
{
    _flags = 0;
}

CommandRule::CommandRule(const String& n, const GemClass &gc, int f, const String& args)
: Linkable()
{
    _name = n;
    _gemClass = gc;
    _flags = f;
    _argTypes = args;
}

int CommandRule::modifyFlags(int setFlags, int clearFlags)
{
    _flags |= setFlags;
    _flags &= ~clearFlags;
    return _flags;
}

/**
 * Defaults to ArgShard.
 */
ArgType CommandRule::argType(int zeroBasedIndex) const
{
    return interpretArgType(_argTypes, zeroBasedIndex);
}
