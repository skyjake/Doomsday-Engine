/** @file classicworldrenderer.h  World renderer used in Doomsday 1/2.
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

#include "iworldrenderer.h"

/**
 * World renderer used in Doomsday up until version 2.
 *
 * The classic world renderer uses the GPU mostly for rasterization. World
 * surfaces are accurately culled based on visibility. DGL is used as the
 * drawing API: it is limited to two textures at once, but batches multiple
 * primitives to larger draw calls.
 *
 * The classic world renderer sorts the visible surfaces based on textures,
 * so that many primitives can be drawn at once.
 */
class ClassicWorldRenderer : public IWorldRenderer
{
public:
    ClassicWorldRenderer();

    void glInit() override;
    void glDeinit() override;
    void loadMap(const de::String &mapId) override;
    void unloadMap() override;
    void setCamera() override;

    void advanceTime(de::TimeSpan elapsed) override;
    void renderPlayerView(int num) override;
};
