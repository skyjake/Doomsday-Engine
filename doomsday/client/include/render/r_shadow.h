/** @file r_shadow.h Map Object Shadows.
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

#ifndef LIBDENG_REFRESH_MOBJ_SHADOW_H
#define LIBDENG_REFRESH_MOBJ_SHADOW_H

#include "dd_types.h"
#include <de/Vector>
#include <de/vector1.h>

struct mobj_s;
class BspLeaf;
class Plane;

/**
 * ShadowProjection. Shadow Projection (POD) stores the results of projection.
 */
typedef struct {
    float s[2], t[2];
    float alpha;
} shadowprojection_t;

/**
 * To be called after map load to initialize this module in preparation for
 * rendering view(s) of the game world.
 */
void R_InitShadowProjectionListsForMap(void);

/**
 * To be called when begining a new render frame to perform necessary initialization.
 */
void R_InitShadowProjectionListsForNewFrame(void);

float R_ShadowAttenuationFactor(coord_t distance);

/**
 * Project all mobj shadows affecting the given quad (world space), calculate
 * coordinates (in texture space) then store into a new list of projections.
 *
 * @pre The coordinates of the given quad must be contained wholly within
 * the BSP leaf specified. This is due to an optimization within the mobj
 * management which separates them according to their position in the BSP.
 *
 * @param bspLeaf BspLeaf within which the quad wholly resides.
 * @param blendFactor  Multiplied with projection alpha.
 * @param topLeft  Top left coordinates of the surface being projected to.
 * @param bottomRight  Bottom right coordinates of the surface being projected to.
 * @param tangent  Normalized tangent of the surface being projected to.
 * @param bitangent  Normalized bitangent of the surface being projected to.
 * @param normal  Normalized normal of the surface being projected to.
 *
 * @return  Projection list identifier if surface is lit else @c 0.
 */
uint R_ProjectShadowsToSurface(BspLeaf *bspLeaf, float blendFactor,
    pvec3d_t topLeft, pvec3d_t bottomRight,
    de::Vector3f const &tangent, de::Vector3f const &bitangent, de::Vector3f const &normal);

/**
 * Iterate over projections in the identified shadow-projection list, making
 * a callback for each visited. Iteration ends when all selected projections
 * have been visited or a callback returns non-zero.
 *
 * @param listIdx  Unique identifier of the list to process.
 * @param callback  Callback to make for each visited projection.
 * @param parameters  Passed to the callback.
 *
 * @return  @c 0 iff iteration completed wholly.
 */
int R_IterateShadowProjections2(uint listIdx, int (*callback) (shadowprojection_t const *, void *), void *parameters);
int R_IterateShadowProjections(uint listIdx, int (*callback) (shadowprojection_t const *, void *)); /* parameters = NULL */

/**
 * Find the highest plane beneath @a mobj onto which it's shadow should be cast.
 * Used with the simple, non-projective method for mobj shadows.
 *
 * @return  Found plane else @c NULL if @a mobj is not presently sector-linked.
 */
Plane *R_FindShadowPlane(struct mobj_s *mobj);

#endif /* LIBDENG_REFRESH_SHADOW_H */
