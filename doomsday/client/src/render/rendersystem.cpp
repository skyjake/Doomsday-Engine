/** @file rendersystem.cpp  Renderer subsystem.
 *
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#include "de_platform.h"
#include "clientapp.h"
#include "render/rendersystem.h"

#include <de/memory.h>

using namespace de;

Store::Store() : posCoords(0), colorCoords(0), vertCount(0), vertMax(0)
{
    zap(texCoords);
}

Store::~Store()
{
    clear();
}

void Store::rewind()
{
    vertCount = 0;
}

void Store::clear()
{
    vertCount = vertMax = 0;

    M_Free(posCoords); posCoords = 0;
    M_Free(colorCoords); colorCoords = 0;

    for(int i = 0; i < NUM_TEXCOORD_ARRAYS; ++i)
    {
        M_Free(texCoords[i]); texCoords[i] = 0;
    }
}

uint Store::allocateVertices(uint count)
{
    uint const base = vertCount;

    // Do we need to allocate more memory?
    vertCount += count;
    while(vertCount > vertMax)
    {
        if(vertMax == 0)
        {
            vertMax = 16;
        }
        else
        {
            vertMax *= 2;
        }

        posCoords = (Vector4f *) M_Realloc(posCoords, sizeof(*posCoords) * vertMax);
        colorCoords = (Vector4ub *) M_Realloc(colorCoords, sizeof(*colorCoords) * vertMax);
        for(int i = 0; i < NUM_TEXCOORD_ARRAYS; ++i)
        {
            texCoords[i] = (Vector2f *) M_Realloc(texCoords[i], sizeof(Vector2f) * vertMax);
        }
    }

    return base;
}

DENG2_PIMPL(RenderSystem)
{
    Store buffer;        ///< Primary map geometry buffer.
    DrawLists drawLists;

    Instance(Public *i) : Base(i) {}
};

RenderSystem::RenderSystem() : d(new Instance(this))
{}

void RenderSystem::timeChanged(Clock const &)
{
    // Nothing to do.
}

Store &RenderSystem::buffer()
{
    return d->buffer;
}

void RenderSystem::clearDrawLists()
{
    d->drawLists.clear();

    // Clear the global vertex buffer, also.
    d->buffer.clear();
}

void RenderSystem::resetDrawLists()
{
    d->drawLists.reset();

    // Start reallocating storage from the global vertex buffer, also.
    d->buffer.rewind();
}

DrawLists &RenderSystem::drawLists()
{
    return d->drawLists;
}
