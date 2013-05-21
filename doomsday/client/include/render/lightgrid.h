/** @file render/lightgrid.h Light Grid (Smoothed ambient sector lighting).
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_RENDER_LIGHT_GRID_H
#define DENG_RENDER_LIGHT_GRID_H

#include "dd_types.h"

#include <de/Vector>

class GameMap;
class Sector;

namespace de {

/**
 * Simple global illumination method utilizing a 2D grid of light levels, achieving
 * smoothed ambient sector lighting.
 *
 * @ingroup render
 */
class LightGrid
{
public:
    /**
     * Two dimensioned reference to a block in the grid [X, Y].
     */
    typedef Vector2i Ref;

    /**
     * Linear reference to a block in the grid (X + Y * Grid Width).
     */
    typedef int Index;

public:
    /**
     * Construct and initialize a new LightGrid for the specifed map.
     *
     * @note Initialization may take some time depending on the complexity of the map
     * (world dimensions, number of sectors) and should therefore be done "off-line".
     */
    LightGrid(GameMap &map);

    /**
     * Register the console comands and variables of this module.
     */
    static void consoleRegister();

    /**
     * Update the grid by finding the strongest light source in each grid block.
     * Should be called periodically to update/refresh the grid contents (e.g., when
     * beginning a new render frame).
     */
    void update();

    /**
     * To be called when an engine variable which affects the lightgrid changes.
     */
    void markAllForUpdate();

    /**
     * Returns the dimensions of the light grid in blocks. For informational purposes.
     */
    Vector2i const &dimensions() const;

    /**
     * Returns the size of grid block in map coordinate space units. For informational
     * purposes.
     */
    coord_t blockSize() const;

    /**
     * Determine the ambient light color for a point in the map coordinate space.
     *
     * @param point  3D point.
     *
     * @return  Evaluated color at the specified point.
     */
    Vector3f evaluate(Vector3d const &point);

    /**
     * Determine the ambient light level for a point in the map coordinate space.
     *
     * @param point  3D point.
     *
     * @return  Evaluated light level at the specified point.
     */
    float evaluateLightLevel(Vector3d const &point);

    /**
     * To be called when the ambient lighting properties in the sector change.
     *
     * @todo Replace with internal de::Observers based mechanism.
     */
    void sectorChanged(Sector &sector);

    /**
     * Draw the light grid debug visual.
     */
    void drawDebugVisual();

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // DENG_RENDER_LIGHT_GRID_H
