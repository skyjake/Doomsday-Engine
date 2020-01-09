/** @file blockmapdebugvisual.h
 *
 * @authors Copyright (c) 2003-2020 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright (c) 2006-2016 Daniel Swanson <danij@dengine.net>
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

#include <doomsday/world/blockmap.h>

class BlockmapDebugVisual
{
public:
    /**
     * Render a visual for this gridmap to assist in debugging (etc...).
     *
     * This visualizer assumes that the caller has already configured the GL
     * render state (projection matrices, scale, etc...) as desired prior to
     * calling. This function guarantees to restore the previous GL state if
     * any changes are made to it.
     *
     * @note Internally this visual uses fixed unit dimensions [1x1] for cells,
     * therefore the caller should scale the appropriate matrix to scale this
     * visual as desired.
     */
    static void draw(const world::Blockmap &);
};
