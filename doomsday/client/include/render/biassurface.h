/** @file biassurface.h Shadow Bias surface.
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

#ifndef DENG_RENDER_SHADOWBIAS_SURFACE_H
#define DENG_RENDER_SHADOWBIAS_SURFACE_H

#include <de/Vector>

#include "MapElement"

class BiasSource;
class BiasTracker;

/**
 * Stores surface data for the Shadow Bias lighting model (e.g., per-vertex
 * illumination and casted source lighting contributions).
 */
class BiasSurface
{
public:
    /**
     * Construct a new surface.
     *
     * @param size          Number of vertices.
     */
    BiasSurface(int size);

    /**
     * To be called to register the commands and variables of this module.
     */
    static void consoleRegister();

    uint lastUpdateOnFrame() const;

    void setLastUpdateOnFrame(uint newLastUpdateFrameNumber);

    void clearAffected();

    void addAffected(float intensity, BiasSource *source);

    void updateAffection(BiasTracker &changes);

    void updateAfterMove();

    /**
     * Perform lighting for the supplied geometry. It is assumed that this
     * geometry has the @em same number of vertices as the bias surface.
     *
     * @param vertCount  Number of vertices to be lit.
     * @param positions  World coordinates for each vertex.
     * @param colors     Final lighting values will be written here.
     */
    void lightPoly(de::Vector3f const &surfaceNormal, int vertCount,
                   struct rvertex_s const *positions, struct ColorRawf_s *colors);

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_RENDER_SHADOWBIAS_SURFACE_H
