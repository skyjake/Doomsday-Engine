/**\file rend_list.h
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
 * Rendering Lists.
 */

#ifndef LIBDENG_REND_LIST_H
#define LIBDENG_REND_LIST_H

#include "r_data.h"

// Multiplicative blending for dynamic lights?
#define IS_MUL              (dlBlend != 1 && !usingFog)

// Types of rendering primitives.
typedef enum primtype_e {
    PT_FIRST = 0,
    PT_TRIANGLE_STRIP = PT_FIRST, // Used for most stuff.
    PT_FAN,
    NUM_PRIM_TYPES
} primtype_t;

// Special rendpoly types.
typedef enum {
    RPT_NORMAL = 0,
    RPT_SKY_MASK, // A sky mask polygon.
    RPT_LIGHT, // A dynamic light.
    RPT_SHADOW, // An object shadow or fakeradio edge shadow.
    RPT_SHINY // A shiny polygon.
} rendpolytype_t;

typedef struct rtexmapuint_s {
    DGLuint         tex;
    int             magMode;
    float           blend;
    float           scale[2], offset[2]; // For use with the texture matrix.
    blendmode_t     blendMode;
} rtexmapunit_t;

extern int renderTextures;
extern int renderWireframe;
extern int useMultiTexLights;
extern int useMultiTexDetails;
extern float detailFactor, detailScale;
extern int torchAdditive;
extern float torchColor[];

void            RL_Register(void);
void            RL_Init(void);
boolean         RL_IsMTexLights(void);
boolean         RL_IsMTexDetails(void);

void            RL_ClearLists(void);
void            RL_DeleteLists(void);

void RL_AddPoly(primtype_t type, rendpolytype_t polyType,
                const rvertex_t* rvertices,
                const rtexcoord_t* rtexcoords, const rtexcoord_t* rtexcoords1,
                const rtexcoord_t* rtexcoords2,
                const rcolor_t* rcolors,
                uint numVertices, uint numLights,
                DGLuint modTex, float modColor[3],
                const rtexmapunit_t rTU[NUM_TEXMAP_UNITS]);
void            RL_RenderAllLists(void);

#endif /* LIBDENG_REND_LIST_H */
