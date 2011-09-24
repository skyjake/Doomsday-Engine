/**\file rend_dyn.h
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
 * Dynamic Light Projections and Projection Lists.
 */

#ifndef LIBDENG_RENDER_DYNLIGHT_H
#define LIBDENG_RENDER_DYNLIGHT_H

/**
 * @defgroup dynlightProjectFlags  Flags for DL_ProjectOnSurface.
 * @{
 */
#define DLF_SORT_LUMINOUSE_DESC 0x1 /// Sort by descending luminosity, brightest to dullest.
#define DLF_NO_PLANAR           0x2 /// Surface is not lit by planar lights.
#define DLF_TEX_FLOOR           0x4 /// Prefer the "floor" slot when picking textures.
#define DLF_TEX_CEILING         0x8 /// Prefer the "ceiling" slot when picking textures.
/**@}*/

/**
 * The data of a projected dynamic light is stored in this structure.
 * A list of these is associated with each surface lit by texture mapped lights
 * in a frame.
 */
typedef struct dynlight_s {
    DGLuint texture;
    float s[2], t[2];
    float color[3];
} dynlight_t;

/**
 * Initialize the dynlight system in preparation for rendering view(s) of the
 * game world. Called by R_InitLevel().
 */
void DL_InitForMap(void);

/**
 * Moves all used dynlight nodes to the list of unused nodes, so they
 * can be reused.
 */
void DL_InitForNewFrame(void);

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
 * @param flags  @see dynlightProjectFlags
 *
 * @return  Dynlight list name if the quad is lit by one or more light sources else @c 0.
 */
uint DL_ProjectOnSurface(subsector_t* ssec, const vectorcomp_t topLeft[3],
    const vectorcomp_t bottomRight[3], const vectorcomp_t normal[3], int flags);

/**
 * Calls func for all projected dynlights in the given list.
 *
 * @param listIdx  Identifier of the list to process.
 * @param data  Ptr to pass to the callback.
 * @param func  Callback to make for each object.
 *
 * @return  @c true, iff every callback returns @c true.
 */
boolean DL_ListIterator(uint listIdx, void* data, boolean (*func) (const dynlight_t*, void*));

#endif /* LIBDENG_RENDER_DYNLIGHT_H */
