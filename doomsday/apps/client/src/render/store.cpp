/** @file store.cpp  Store.
 *
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#include "render/store.h"

#include <de/libcore.h>
#include <de/legacy/memory.h>

using namespace de;

Store::Store()
{
    de::zap(texCoords);
}

Store::~Store()
{
    clear();
}

void Store::rewind()
{
    _vertCount = 0;
}

void Store::clear()
{
    _vertCount = _vertMax = 0;

    M_Free(posCoords); posCoords = nullptr;
    M_Free(colorCoords); colorCoords = nullptr;
    for(dint i = 0; i < 2; ++i)
    {
        M_Free(texCoords[i]); texCoords[i] = nullptr;
    }
    M_Free(modCoords); modCoords = nullptr;
}

duint Store::allocateVertices(duint count)
{
    const duint base = _vertCount;

    // Do we need to allocate more memory?
    _vertCount += count;
    while(_vertCount > _vertMax)
    {
        if(_vertMax == 0)
        {
            _vertMax = 16;
        }
        else
        {
            _vertMax *= 2;
        }

        posCoords   = (Vec3f *)  M_Realloc(posCoords,   sizeof(*posCoords) * _vertMax);
        colorCoords = (Vec4ub *) M_Realloc(colorCoords, sizeof(*colorCoords) * _vertMax);
        for(dint i = 0; i < 2; ++i)
        {
            texCoords[i] = (Vec2f *) M_Realloc(texCoords[i], sizeof(Vec2f) * _vertMax);
        }
        modCoords   = (Vec2f *) M_Realloc(modCoords,  sizeof(*modCoords) * _vertMax);
    }

    return base;
}
