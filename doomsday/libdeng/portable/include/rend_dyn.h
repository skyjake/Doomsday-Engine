/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * rend_dyn.h: Dynamic Lights.
 */

#ifndef __DOOMSDAY_DYNLIGHTS_H__
#define __DOOMSDAY_DYNLIGHTS_H__

#include "p_object.h"
#include "rend_list.h"

#define DYN_ASPECT          (1.1f) // 1.2f is just too round for Doom.

#define MAX_GLOWHEIGHT      (1024.0f) // Absolute max glow height.

// Flags for DL_ProjectOnSurface
#define DLF_SORT_LUMADSC    0x1 // Sort dynlights by luma, brightest to dullest.
#define DLF_NO_PLANAR       0x2 // Surface is not lit by planar lights.
#define DLF_TEX_FLOOR       0x4 // Prefer the "floor" slot when picking textures.
#define DLF_TEX_CEILING     0x8 // Prefer the "ceiling" slot when picking textures.

/**
 * The data of a projected dynamic light is stored in this structure.
 * A list of these is associated with each surface lit by texture mapped lights
 * in a frame.
 */
typedef struct dynlight_s {
    float           s[2], t[2];
    float           color[3];
    DGLuint         texture;
} dynlight_t;

extern int useDynLights, dlBlend;
extern float dlFactor, dlFogBright;
extern int useWallGlow, glowHeightMax;
extern float glowHeightFactor;

// Initialization
void            DL_Register(void);

// Setup.
void            DL_InitForMap(void);
void            DL_InitForNewFrame(void);

// Action.
uint            DL_ProjectOnSurface(subsector_t* ssec, const vectorcomp_t topLeft[3],
                                    const vectorcomp_t bottomRight[3],
                                    const vectorcomp_t normal[3], byte flags);
// Helpers.
boolean         DL_ListIterator(uint listIdx, void* data,
                                boolean (*func) (const dynlight_t*, void*));

#endif
