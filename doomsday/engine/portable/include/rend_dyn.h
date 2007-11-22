/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2007 Daniel Swanson <danij@dengine.net>
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

#define DYN_ASPECT          1.1f    // 1.2f is just too round for Doom.

#define MAX_GLOWHEIGHT      1024.0f // Absolute max glow height

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

extern int      useDynLights, dlBlend;
extern float    dlFactor;
extern int      useWallGlow, glowHeightMax;
extern float    glowHeightFactor;
extern float    glowFogBright;

// Initialization
void            DL_Register(void);

// Setup.
void            DL_InitForMap(void);
void            DL_InitForNewFrame(void);

// Action.
uint            DL_ProcessSegSection(seg_t *seg, float bottom, float top,
                                     boolean sortBrightestFirst);
uint            DL_ProcessSubSectorPlane(subsector_t *subsector, uint plane);

// Helpers.
boolean         DL_ListIterator(uint listIdx, void *data,
                                boolean (*func) (const dynlight_t *, void *data));

#endif
