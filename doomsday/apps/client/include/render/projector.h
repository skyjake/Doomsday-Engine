/** @file projector.h Texture coordinate projector and projection lists.
 *
 * @todo: This module should be replaced with a system which decentralizes the
 * projection lists such that these can be managed piecewise and possibly used
 * for drawing multiple frames.
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

#ifndef DENG_CLIENT_RENDER_PROJECTOR_H
#define DENG_CLIENT_RENDER_PROJECTOR_H

#include <de/Matrix>
#include <de/Vector>

#include "api_gl.h" // DGLuint

#include "world/map.h"

class ConvexSubspace;

/**
 * Stores data for a texture surface projection.
 */
struct TexProjection
{
    DGLuint texture;
    de::Vector2f topLeft, bottomRight;
    de::Vector4f color;

    TexProjection(DGLuint texture, de::Vector2f const &topLeft,
                  de::Vector2f const &bottomRight, de::Vector4f const &color)
        : texture(texture),
          topLeft(topLeft),
          bottomRight(bottomRight),
          color(color)
    {}
};

/**
 * To be called to initialize the projector when the current map changes.
 */
void Rend_ProjectorInitForMap(de::Map &map);

/**
 * To be called at the start of a render frame to clear the projection lists
 * to prepare for subsequent drawing.
 */
void Rend_ProjectorReset();

/**
 * Project all lumobjs affecting the given quad (world space), calculate
 * coordinates (in texture space) then store into a new list of projections.
 *
 * @pre The coordinates of the given quad must be contained wholly within
 * the subspoce specified. This is due to an optimization within the lumobj
 * management which separates them by subspace.
 *
 * @param subspace         ConvexSubspace within which the quad wholly resides.
 * @param topLeft          Top left coordinates of the surface being projected to.
 * @param bottomRight      Bottom right coordinates of the surface being projected to.
 * @param tangentMatrix    Normalized tangent space matrix of the surface being projected to.
 * @param blendFactor      Multiplied with projection alpha.
 * @param lightmap         Semantic identifier of the lightmap to use.
 * @param sortByLuminance  @c true= Sort the projections by luminosity.
 *
 * Return values:
 * @param listIdx          If projected to, the identifier of the resultant list
 *                         (1-based) is written here. If a projection list already
 *                         exists it will be reused.
 */
void Rend_ProjectLumobjs(ConvexSubspace *subspace, de::Vector3d const &topLeft,
    de::Vector3d const &bottomRight, de::Matrix3f const &tangentMatrix,
    float blendFactor, Lumobj::LightmapSemantic lightmap, bool sortByLuminance,
    uint &listIdx);

/**
 * Project all plane glows affecting the given quad (world space), calculate
 * coordinates (in texture space) then store into a new list of projections.
 *
 * @pre The coordinates of the given quad must be contained wholly within
 * the subspace specified. This is due to an optimization within the lumobj
 * management which separates them by subspace.
 *
 * @param subspace         ConvexSubspace within which the quad wholly resides.
 * @param topLeft          Top left coordinates of the surface being projected to.
 * @param bottomRight      Bottom right coordinates of the surface being projected to.
 * @param tangentMatrix    Normalized tangent space matrix of the surface being projected to.
 * @param blendFactor      Multiplied with projection alpha.
 * @param sortByLuminance  @c true= Sort the projections by luminosity.
 *
 * Return values:
 * @param listIdx          If projected to, the identifier of the resultant list
 *                         (1-based) is written here. If a projection list already
 *                         exists it will be reused.
 */
void Rend_ProjectPlaneGlows(ConvexSubspace *subspace, de::Vector3d const &topLeft,
    de::Vector3d const &bottomRight, de::Matrix3f const &tangentMatrix,
    float blendFactor, bool sortByLuminance, uint &listIdx);

/**
 * Project all mobj shadows affecting the given quad (world space), calculate
 * coordinates (in texture space) then store into a new list of projections.
 *
 * @pre The coordinates of the given quad must be contained wholly within
 * the subspace specified. This is due to an optimization within the mobj
 * management which separates them by subspace.
 *
 * @param subspace       ConvexSubspace within which the quad wholly resides.
 * @param topLeft        Top left coordinates of the surface being projected to.
 * @param bottomRight    Bottom right coordinates of the surface being projected to.
 * @param tangentMatrix  Normalized tangent space matrix of the surface being projected to.
 * @param blendFactor    Multiplied with projection alpha.
 *
 * Return values:
 * @param listIdx        If projected to, the identifier of the resultant list
 *                       (1-based) is written here. If a projection list already
 *                       exists it will be reused.
 */
void Rend_ProjectMobjShadows(ConvexSubspace *subspace, de::Vector3d const &topLeft,
    de::Vector3d const &bottomRight, de::Matrix3f const &tangentMatrix,
    float blendFactor, uint &listIdx);

/**
 * Iterate over projections in the identified surface-projection list, making
 * a callback for each visited. Iteration ends when all selected projections
 * have been visited or a callback returns non-zero.
 *
 * @param listIdx   Unique identifier of the list to process.
 * @param callback  Callback to make for each visited projection.
 * @param context   Passed to the callback.
 *
 * @return  @c 0 iff iteration completed wholly.
 */
int Rend_IterateProjectionList(uint listIdx, int (*callback) (TexProjection const *, void *),
    void *context = 0);

#endif // DENG_CLIENT_RENDER_PROJECTOR_H
