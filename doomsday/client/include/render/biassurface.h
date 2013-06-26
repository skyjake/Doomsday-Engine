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
     * @param owner         Map element which will own the surface (either a
     *                      BspLeaf or a Segment).
     * @param subElemIndex  Index for the subelement of @a owner.
     * @param size          Number of vertices.
     */
    BiasSurface(de::MapElement &owner, int subElemIndex, int size);

    /**
     * To be called to register the commands and variables of this module.
     */
    static void consoleRegister();

    /**
     * Perform lighting for the supplied geometry. It is assumed that this
     * geometry has the @em same number of vertices as the bias surface.
     *
     * @param colors            Array of colors to be written to.
     * @param verts             Array of vertices to be lit.
     * @param vertCount         Number of vertices (in the array) to be lit.
     * @param sectorLightLevel  Sector light level.
     */
    void lightPoly(struct ColorRawf_s *colors, struct rvertex_s const *verts,
                   int vertCount, float sectorLightLevel);

    void updateAfterMove();

    void updateAffection(BiasTracker &changes);

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_RENDER_SHADOWBIAS_SURFACE_H
