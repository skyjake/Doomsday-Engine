/** @file biastracker.h Shadow Bias change tracking buffer.
 *
 * @authors Copyright © 2005-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "render/biastracker.h"

using namespace de;

void BiasTracker::mark(uint index)
{
    // Assume 32-bit uint.
    //if(index >= 0)
    {
        _changes[index >> 5] |= (1 << (index & 0x1f));
    }
    /*else
    {
        _changes[(-index) >> 5] &= ~(1 << ((-index) & 0x1f));
    }*/
}

int BiasTracker::check(uint index) const
{
    // Assume 32-bit uint.
    return (_changes[index >> 5] & (1 << (index & 0x1f))) != 0;
}

void BiasTracker::apply(BiasTracker const &src)
{
    for(int i = 0; i < MAX_TRACKED; ++i)
    {
        _changes[i] |= src._changes[i];
    }
}

void BiasTracker::clear(BiasTracker const &src)
{
    for(int i = 0; i < MAX_TRACKED; ++i)
    {
        _changes[i] &= ~src._changes[i];
    }
}
