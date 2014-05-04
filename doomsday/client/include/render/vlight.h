/** @file vlight.h  Vector light sources and source lists.
 *
 * @authors Copyright © 2003-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef DENG_CLIENT_RENDER_VECTORLIGHT_H
#define DENG_CLIENT_RENDER_VECTORLIGHT_H

#include <de/Vector>

#include "world/map.h"

class ConvexSubspace;

/**
 * Vector light source.
 * @ingroup render
 */
struct VectorLight
{
    float approxDist;          ///< Only an approximation.
    de::Vector3f direction;    ///< Normalized vector from light origin to illumination point.
    de::Vector3f color;        ///< How intense the light is (0..1, RGB).
    float offset;
    float lightSide, darkSide; ///< Factors for world light.
    bool affectedByAmbient;
};

/**
 * Initialize the vlight system in preparation for rendering view(s) of the
 * game world.
 */
void VL_InitForMap(de::Map &map);

/**
 * Moves all used vlight nodes to the list of unused nodes, so they can be
 * reused.
 */
void VL_InitForNewFrame();

struct collectaffectinglights_params_t
{
    de::Vector3d origin;
    de::Vector3f ambientColor;
    ConvexSubspace *subspace;
    bool starkLight; ///< World light has a more pronounced effect.
};

uint R_CollectAffectingLights(collectaffectinglights_params_t const *params);

/**
 * Calls func for all vlights in the given list.
 *
 * @param listIdx   Unique identifier of the list to process.
 * @param callback  Callback to make for each visited projection.
 * @param context   Passed to the callback.
 *
 * @return  @c 0 iff iteration completed wholly.
 */
int VL_ListIterator(uint listIdx, int (*callback) (VectorLight const *, void *),
                    void *context = 0);

void Rend_DrawVectorLight(VectorLight const *vlight, float alpha);

#endif // DENG_CLIENT_RENDER_VECTORLIGHT_H
