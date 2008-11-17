/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
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
 * rend_list.h: Rendering Lists
 */

#ifndef __DOOMSDAY_REND_LIST_H__
#define __DOOMSDAY_REND_LIST_H__

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

// Helper macro for accessing texture map units.
#define TMU(x, n)          (&((x)->texmapunits[(n)]))

typedef enum {
    TMU_PRIMARY = 0,
    TMU_PRIMARY_DETAIL,
    TMU_INTER,
    TMU_INTER_DETAIL,
    NUM_TEXMAP_UNITS
} texmapunit_t;

typedef struct rtexmapuint_s {
    DGLuint         tex;
    int             magMode;
    float           scale[2], offset[2]; // For use with the texture matrix.
} rtexmapunit_t;

// rladdpoly_params_t is only for convenience; the data written in the rendering
// list data buffer is taken from this struct.
typedef struct rladdpoly_params_s {
    rendpolytype_t  type;
    rtexmapunit_t   texmapunits[NUM_TEXMAP_UNITS];
    float           interPos; // Blending strength (0..1).
    DGLuint         modTex;
    float           modColor[3];
} rladdpoly_params_t;

extern int renderTextures;
extern int renderWireframe;
extern int useMultiTexLights;
extern int useMultiTexDetails;
extern float rendLightWallAngle;
extern float detailFactor, detailScale;
extern int torchAdditive;
extern float torchColor[];

void            RL_Register(void);
void            RL_Init(void);
boolean         RL_IsMTexLights(void);
boolean         RL_IsMTexDetails(void);

void            RL_ClearLists(void);
void            RL_DeleteLists(void);

void            RL_AddPoly(primtype_t type, const rvertex_t* vertices,
                           const rtexcoord_t* rtexcoords,
                           const rtexcoord_t* rtexcoords2,
                           const rtexcoord_t* rtexcoords5,
                           const rcolor_t* colors, uint numVertices,
                           blendmode_t blendMode, boolean isLit,
                           const rladdpoly_params_t* params);
void            RL_AddMaskedPoly(const rvertex_t* vertices,
                                 const rcolor_t* colors, float wallLength,
                                 float texWidth, float texHeight,
                                 const float texOffset[2],
                                 blendmode_t blendMode,
                                 uint lightListIdx, boolean glow,
                                 boolean masked,
                                 const rladdpoly_params_t* params);
void            RL_RenderAllLists(void);

void            RL_FloatRGB(byte* rgb, float* dest);

#endif
