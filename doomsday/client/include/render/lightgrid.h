/** @file lightgrid.h  Light Grid (Smoothed ambient sector lighting).
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_CLIENT_LIGHTGRID_H
#define DENG_CLIENT_LIGHTGRID_H

#include <de/libdeng2.h>
#include <de/Error>
#include <de/Vector>
#include "render/ilightsource.h"

namespace de {

class Map;

/**
 * Simple global illumination method utilizing a 2D grid of block light sources,
 * achieving smoothed ambient lighting for sectors.
 *
 * @ingroup render
 */
class LightGrid
{
public:
    /**
     * Linear reference to a block in the grid (X + Y * Grid Width).
     */
    typedef int Index;

    /**
     * Two dimensioned reference to a block in the grid [X, Y].
     */
    typedef Vector2i Ref;

    /**
     * Base class for a block illumination source.
     *
     * Derived classes are obliged to call @ref LightGrid::blockLightSourceChanged()
     * whenever the properties of the light source change, so that any necessary
     * updates can be scheduled.
     */
    class IBlockLightSource : public ILightSource
    {
    public:
        virtual ~IBlockLightSource() {}

        /**
         * Determines the Z-axis bias scale factor for the illumination source.
         */
        virtual int blockLightSourceZBias() = 0;
    };

public:
    /**
     * Construct and initialize an empty LightGrid.
     */
    LightGrid(Vector2d const &origin, Vector2d const &dimensions);

    /**
     * To be called when the physical dimensions of the grid or the logical block
     * size changes to resize the light grid. Note that resizing inherently means
     * clearing of primary illumination sources, so they'll need to be initialized
     * again.
     *
     * @param newOrigin      Origin of the grid in map space.
     * @param newDimensions  Dimensions of the grid in map space units.
     */
    void resizeAndClear(Vector2d const &newOrigin, Vector2d const &newDimensions);

    /**
     * Determine the smoothed ambient lighting properties for the given @a point
     * in the map coordinate space.
     *
     * @param point  3D point.
     *
     * @return  Evaluated color at the specified point.
     * - [x, y, z] = RGB color with premultiplied luminance factor
     * - [w]       = luminance factor (i.e., light level)
     */
    Vector4f evaluate(Vector3d const &point);

    /**
     * Convenient method of determining the smoothed ambient lighting properties
     * for the given @a point in the map coordinate space and returns the intensity
     * factor directly.
     *
     * @param point  3D point.
     *
     * @return  Evaluated light intensity at the specified point.
     *
     * @see evaluate
     */
    inline float evaluateIntensity(Vector3d const &point) {
        return evaluate(point).w;
    }

    /**
     * To be called when an engine variable which affects the lightgrid changes.
     * @todo Only needed because of the sky light color. -ds
     */
    void scheduleFullUpdate();

    /**
     * Update the grid. Should be called periodically to update/refresh the grid
     * contents (e.g., when beginning a new render frame).
     */
    void updateIfNeeded();

    /**
     * Change the primary light source for the specified grid @a block. Whenever
     * a primary source is changed the necessary grid updates are scheduled.
     *
     * @param index      Linear index of the block to change.
     * @param newSource  New primary light source to set. Use @c 0 to clear.
     */
    void setPrimarySource(Index block, IBlockLightSource *newSource);

    /**
     * Lookup the primary illumination source for the specified @a block. For debug.
     */
    IBlockLightSource *primarySource(Index block) const;

    /**
     * IBlockLightSources are obliged to call this whenever the attributes of the
     * the light source have changed to schedule any necessary grid updates.
     */
    void blockLightSourceChanged(IBlockLightSource *changedSource);

    /**
     * Register the console comands and variables of this module.
     */
    static void consoleRegister();

public: // Utilities -----------------------------------------------------------

    /// Returns the linear grid index for the given two-dimensioned grid reference.
    inline Index toIndex(int x, int y)    { return y * dimensions().x + x; }

    /// @copydoc toIndex()
    inline Index toIndex(Ref const &gref) { return toIndex(gref.x, gref.y); }

    /// Returns the two-dimensioned grid reference for the given map space @a point.
    Ref toRef(Vector3d const &point);

    /// Returns the size of a grid block in map space units.
    int blockSize() const;

    /// Returns the origin of the grid in map space.
    Vector2d const &origin() const;

    /// Returns the dimensions of the grid in blocks.
    Vector2i const &dimensions() const;

    /// Returns the total number of non-null blocks in the grid.
    int numBlocks() const;

    /// Returns the total number of bytes used for non-null blocks in the grid.
    size_t blockStorageSize() const;

    /// Returns the "raw" color for the specified @a block. For debug.
    Vector3f const &rawColorRef(Index block) const;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // DENG_CLIENT_LIGHTGRID_H
