/**
 * @file vlight.h Vector light
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_RENDER_VLIGHT_H
#define LIBDENG_RENDER_VLIGHT_H

#include "map/bspleaf.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Vector light.
 * @ingroup render
 */
typedef struct
{
    float approxDist; // Only an approximation.
    float vector[3]; // Light direction vector.
    float color[3]; // How intense the light is (0..1, RGB).
    float offset;
    float lightSide, darkSide; // Factors for world light.
    boolean affectedByAmbient;
} vlight_t;

boolean R_DrawVLightVector(vlight_t const *light, void *context);

/**
 * Initialize the vlight system in preparation for rendering view(s) of the
 * game world.
 */
void VL_InitForMap(void);

/**
 * Moves all used vlight nodes to the list of unused nodes, so they can be
 * reused.
 */
void VL_InitForNewFrame(void);

/**
 * Calls func for all vlights in the given list.
 *
 * @param listIdx  Identifier of the list to process.
 * @param data  Ptr to pass to the callback.
 * @param func  Callback to make for each object.
 *
 * @return  @c true, iff every callback returns @c true.
 */
boolean VL_ListIterator(uint listIdx, void *data, boolean (*func) (vlight_t const *, void *));

typedef struct collectaffectinglights_params_s {
    coord_t origin[3];
    float *ambientColor;
    BspLeaf *bspLeaf;
    boolean starkLight; ///< World light has a more pronounced effect.
} collectaffectinglights_params_t;

uint R_CollectAffectingLights(collectaffectinglights_params_t const *params);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_RENDER_VLIGHT_H */
