/** @file irenderer.h  Interface for a world renderer implementation.
 *
 * @authors Copyright (c) 2020 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#pragma once

#include <de/string.h>
#include <de/time.h>

/**
 * Interface for a world renderer implementation.
 *
 * This is the highest level API for the world renderer.
 *
 * @ingroup render
 */
class IWorldRenderer
{
public:
    virtual ~IWorldRenderer() = default;

    virtual void glInit() = 0;
    virtual void glDeinit() = 0;

    virtual void loadMap(const de::String &mapId) = 0;
    virtual void unloadMap() = 0;
    virtual void setCamera() = 0;

    virtual void advanceTime(de::TimeSpan elapsed) = 0;
    virtual void renderPlayerView(int num) = 0;
};
