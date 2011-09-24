/**\file r_textureprojection.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * Texture Projections and Surface-Projection Lists
 */

#ifndef LIBDENG_REFRESH_TEXTUREPROJECTION_H
#define LIBDENG_REFRESH_TEXTUREPROJECTION_H

/**
 * @defgroup surfaceProjectFlags  Flags for R_ProjectOnSurface.
 * @{
 */
#define DLF_SORT_LUMINOUSE_DESC 0x1 /// Sort by descending luminosity, brightest to dullest.
#define DLF_NO_PLANE           0x2 /// Surface is not lit by planar lights.
#define DLF_TEX_FLOOR           0x4 /// Prefer the "floor" slot when picking textures.
#define DLF_TEX_CEILING         0x8 /// Prefer the "ceiling" slot when picking textures.
/**@}*/

/**
 * The data of a projected dynamic light is stored in this structure.
 * A list of these is associated with each surface lit by texture mapped
 * lights in a frame.
 */
typedef struct {
    DGLuint texture;
    float s[2], t[2];
    float color[3];
} textureprojection_t;

/**
 * Initialize the surface projection system in preparation prior to rendering
 * view(s) of the game world.
 */
void R_InitSurfaceProjectionListsForMap(void);

/**
 * Initialize the surface projection system to begin a new refresh frame. 
 */
void R_InitSurfaceProjectionListsForNewFrame(void);

/**
 * Project all objects affecting the given quad (world space), calculate
 * coordinates (in texture space) then store into a new list of projections.
 *
 * \assume The coordinates of the given quad must be contained wholly within
 * the subsector specified. This is due to an optimization within the lumobj
 * management which separates them according to their position in the BSP.
 *
 * @param ssec  Subsector within which the quad wholly resides.
 * @param topLeft  Coordinates of the top left corner of the quad.
 * @param bottomRight  Coordinates of the bottom right corner of the quad.
 * @param normal  Normalized normal of the quad.
 * @param flags  @see surfaceProjectFlags
 *
 * @return  Dynlight list name if the quad is lit by one or more light sources else @c 0.
 */
uint R_ProjectOnSurface(subsector_t* ssec, const vectorcomp_t topLeft[3],
    const vectorcomp_t bottomRight[3], const vectorcomp_t normal[3], int flags);

/**
 * Iterate over projections in the identified surface-projection list, making
 * a callback for each visited. Iteration ends when all selected projections
 * have been visited or a callback returns non-zero.
 *
 * @param listIdx  Unique identifier of the list to process.
 * @param callback  Callback to make for each visited projection.
 * @param paramaters  Passed to the callback.
 *
 * @return  @c 0 iff iteration completed wholly.
 */
int R_IterateSurfaceProjections2(uint listIdx, int (*callback) (const textureprojection_t*, void*), void* paramaters);
int R_IterateSurfaceProjections(uint listIdx, int (*callback) (const textureprojection_t*, void*)); /* paramaters = NULL */

#endif /* LIBDENG_REFRESH_TEXTUREPROJECTION_H */
