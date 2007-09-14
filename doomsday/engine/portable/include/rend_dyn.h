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

/*
 * rend_dyn.h: Dynamic Lights
 */

#ifndef __DOOMSDAY_DYNLIGHTS_H__
#define __DOOMSDAY_DYNLIGHTS_H__

#include "p_object.h"
#include "rend_list.h"

#define DYN_ASPECT          1.1f    // 1.2f is just too round for Doom.

#define MAX_GLOWHEIGHT      1024.0f // Absolute max glow height

// Lumobj Flags.
#define LUMF_USED           0x1
#define LUMF_RENDERED       0x2
#define LUMF_CLIPPED        0x4     // Hidden by world geometry.
#define LUMF_NOHALO         0x100
#define LUMF_DONTTURNHALO   0x200

// Lumobject types.
typedef enum {
    LT_OMNI,                        // Omni (spherical) light.
    LT_PLANE                        // Planar light.
} lumtype_t;

// Helper macros for accessing lum data.
#define LUM_OMNI(x)         (&((x)->data.omni))
#define LUM_PLANE(x)        (&((x)->data.plane))

typedef struct lumobj_s {           // For dynamic lighting.
    lumtype_t       type;
    int             flags;          // LUMF_* flags.
    float           pos[3];         // Center of the light. 
    float           color[3];
    float           distanceToViewer;
    subsector_t    *subsector;

    union lumobj_data_u {
        struct lumobj_omni_s {
            int             radius;         // Radius for this omnilight source.
            float           zOff;           // Offset to center from pos[VZ].
            DGLuint         tex;            // Lightmap texture.
            DGLuint         floorTex, ceilTex;  // Lightmaps for floor/ceil.
            DGLuint         decorMap;       // Decoration lightmap.

        // For flares (halos).
            int             flareSize;    
            byte            halofactor;
            float           xOff;
            DGLuint         flareTex;       // Flaremap if flareCustom ELSE (flaretexName id.
                                            // Zero = automatical)
            boolean         flareCustom;    // True id flareTex is a custom flare graphic
            float           flareMul;       // Flare brightness factor.
        } omni;
        struct lumobj_plane_s {
            float           intensity;
            DGLuint         tex;
            float           normal[3];
        } plane;
    } data;
} lumobj_t;

/*
 * The data of a projected dynamic light is stored in this structure.
 * A list of these is associated with each surface lit by texture mapped lights
 * in a frame.
 */
typedef struct dynlight_s {
    float           s[2], t[2];
    float           color[3];
    DGLuint         texture;
} dynlight_t;

extern boolean  dlInited;
extern int      useDynLights;
extern int      dlBlend, dlMaxRad;
extern float    dlRadFactor, dlFactor;
extern int      useWallGlow, glowHeightMax;
extern float    glowHeightFactor;
extern float    glowFogBright;
extern byte     rendInfoLums;
extern int      dlMinRadForBias;

// Initialization
void            DL_Register(void);

// Setup.
void            DL_InitForMap(void);
void            DL_Clear(void);        // 'Physically' destroy the tables.

// Action.
void            DL_ClearForFrame(void);
void            DL_InitForNewFrame(void);
uint            DL_NewLuminous(lumtype_t type);
lumobj_t*       DL_GetLuminous(uint idx);
uint            DL_GetNumLuminous(void);
void            DL_InitForSubsector(subsector_t *ssec);
uint            DL_ProcessSegSection(seg_t *seg, float bottom, float top);
uint            DL_ProcessSubSectorPlane(subsector_t *subsector, uint plane);

// Helpers.
boolean         DL_DynLightIterator(uint listIdx, void *data,
                                boolean (*func) (const dynlight_t *, void *data));
boolean         DL_LumRadiusIterator(subsector_t *subsector, fixed_t x, fixed_t y,
                                  fixed_t radius, void *data,
                                  boolean (*func) (lumobj_t *, fixed_t, void *data));

void            DL_ClipInSubsector(uint ssecidx);
void            DL_ClipBySight(uint ssecidx);

#endif
