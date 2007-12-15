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
 * r_lumobjs.h: Lumobj (luminous object) management.
 */

#ifndef __DOOMSDAY_REFRESH_LUMINOUS_H__
#define __DOOMSDAY_REFRESH_LUMINOUS_H__

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

extern boolean  loInited;

extern uint     loMaxLumobjs;
extern int      loMaxRadius;
extern float    loRadiusFactor;
extern int      loMinRadForBias;
extern byte     rendInfoLums;

extern int      useMobjAutoLights;

// Initialization.
void            LO_Register(void);

// Setup.
void            LO_InitForMap(void);
void            LO_Clear(void);        // 'Physically' destroy the tables.

// Action.
void            LO_ClearForFrame(void);
void            LO_InitForNewFrame(void);
uint            LO_NewLuminous(lumtype_t type);
lumobj_t       *LO_GetLuminous(uint idx);
uint            LO_GetNumLuminous(void);
void            LO_InitForSubsector(subsector_t *ssec);

// Helpers.
boolean         LO_IterateSubsectorContacts(subsector_t *ssec,
                                            boolean (*func) (void *, void *data),
                                            void *data);
boolean         LO_LumobjsRadiusIterator(subsector_t *subsector, float x, float y,
                                         float radius, void *data,
                                         boolean (*func) (lumobj_t *, float, void *data));

void            LO_ClipInSubsector(uint ssecidx);
void            LO_ClipBySight(uint ssecidx);

void            LO_DrawLumobjs(void);
#endif
