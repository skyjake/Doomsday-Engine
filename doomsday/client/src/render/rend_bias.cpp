/** @file rend_bias.cpp Shadow Bias lighting model.
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

#include "render/rend_bias.h"

using namespace de;

/**
 * Interpolate between current and destination.
 */
void lerpIllumination(VertexIllum &illum, uint currentTime, int lightSpeed,
                      Vector3f &result)
{
    if(!(illum.flags & VIF_LERP))
    {
        // We're done with the interpolation, just use the destination color.
        result = illum.color;
        return;
    }

    float inter = (currentTime - illum.updateTime) / float( lightSpeed );

    if(inter > 1)
    {
        illum.flags &= ~VIF_LERP;
        illum.color = illum.dest;

        result = illum.color;
    }
    else
    {
        result = illum.color + (illum.dest - illum.color) * inter;
    }
}

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
