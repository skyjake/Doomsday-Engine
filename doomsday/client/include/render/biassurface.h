/** @file biassurface.h Shadow Bias surface.
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

#ifndef DENG_RENDER_SHADOWBIAS_SURFACE_H
#define DENG_RENDER_SHADOWBIAS_SURFACE_H

#include <de/Vector>

#include "render/rendpoly.h" /// @todo remove me

#include "BiasDigest"

/**
 * Base class for a surface which supports lighting within the Shadow Bias
 * lighting model.
 */
class BiasSurface
{
public:
    virtual ~BiasSurface() {}

    /**
     * To be called to register the commands and variables of this module.
     */
    static void consoleRegister();

    /**
     * Perform lighting for the supplied geometry. The derived class wil define
     * how these vertices map to bias illumination points.
     *
     * @param geomGroup  Geometry group identifier.
     * @param positions  World coordinates for each vertex.
     * @param colors     Final lighting values will be written here.
     */
    virtual void lightBiasPoly(int geomGroup, rvertex_t const *positions,
                               de::Vector4f *colors) = 0;

    /**
     * Schedule a lighting update to a geometry group following a move of some
     * other element of dependent geometry. Derived classes may override this
     * with their own update logic. The default implementation does nothing.
     *
     * @param group  Geometry group identifier.
     */
    virtual void updateBiasAfterGeometryMove(int group) {}

    /**
     * @param changes  Digest of bias lighting changes to apply.
     */
    virtual void applyBiasDigest(BiasDigest &changes) = 0;
};

extern int devUpdateBiasContributors; //cvar

#endif // DENG_RENDER_SHADOWBIAS_SURFACE_H
