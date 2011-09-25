/**\file r_lumobjs.h
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
 * Lumobj (luminous object) management.
 */

#ifndef LIBDENG_REFRESH_LUMINOUS_H
#define LIBDENG_REFRESH_LUMINOUS_H

// Lumobject types.
typedef enum {
    LT_OMNI, // Omni (spherical) light.
    LT_PLANE, // Planar light.
} lumtype_t;

// Helper macros for accessing lum data.
#define LUM_OMNI(x)         (&((x)->data.omni))
#define LUM_PLANE(x)        (&((x)->data.plane))

typedef struct lumobj_s {
    lumtype_t type;
    float pos[3]; // Center of the obj.
    subsector_t* subsector;
    float maxDistance;
    void* decorSource; // decorsource_t ptr, else @c NULL.

    union lumobj_data_u {
        struct lumobj_omni_s {
            float color[3];
            float radius; // Radius for this omnilight source.
            float zOff; // Offset to center from pos[VZ].
            DGLuint tex; // Lightmap texture.
            DGLuint floorTex, ceilTex; // Lightmaps for floor/ceil.
        } omni;
        struct lumobj_plane_s {
            float color[3];
            float intensity;
            DGLuint tex;
            float normal[3];
        } plane;
    } data;
} lumobj_t;

extern boolean loInited;

extern uint loMaxLumobjs;
extern int loMaxRadius;
extern float loRadiusFactor;
extern byte rendInfoLums;
extern int useMobjAutoLights;

/// Register the console commands, variables, etc..., of this module.
void LO_Register(void);

// Setup.
void LO_InitForMap(void);

/// Release all system resources acquired by this module for managing.
void LO_Clear(void);

/**
 * To be called at the beginning of a world frame (prior to rendering views of the
 * world), to perform necessary initialization within this module.
 */
void LO_BeginWorldFrame(void);

/**
 * To be called at the beginning of a render frame to perform necessary initialization
 * within this module.
 */
void LO_BeginFrame(void);

/**
 * Create lumobjs for all sector-linked mobjs who want them.
 */
void LO_AddLuminousMobjs(void);

/**
 * To be called when lumobjs are disabled to perform necessary bookkeeping within this module.
 */
void LO_UnlinkMobjLumobjs(void);

/// @return  The number of active lumobjs for this frame.
uint LO_GetNumLuminous(void);

/**
 * Construct a new lumobj and link it into subsector @a ssec.
 * @return  Logical index (name) for referencing the new lumobj.
 */
uint LO_NewLuminous(lumtype_t type, subsector_t* ssec);

/// @return  Lumobj associated with logical index @a idx else @c NULL.
lumobj_t* LO_GetLuminous(uint idx);

/// @return  Logical index associated with lumobj @a lum.
uint LO_ToIndex(const lumobj_t* lum);

/// @return  @c true if the lumobj is clipped for the viewer.
boolean LO_IsClipped(uint idx, int i);

/// @return  @c true if the lumobj is hidden for the viewer.
boolean LO_IsHidden(uint idx, int i);

/// @return  Approximated distance between the lumobj and the viewer.
float LO_DistanceToViewer(uint idx, int i);

/**
 * Calculate a distance attentuation factor for a lumobj.
 *
 * @param idx  Logical index associated with the lumobj.
 * @param distance  Distance between the lumobj and the viewer.
 *
 * @return  Attentuation factor [0...1]
 */
float LO_AttenuationFactor(uint idx, float distance);

/**
 * Clip lumobj, omni lights in the given subsector.
 *
 * @param ssecidx  Subsector index in which lights will be clipped.
 */
void LO_ClipInSubsector(uint ssecidx);

/**
 * In the situation where a subsector contains both lumobjs and a polyobj,
 * the lumobjs must be clipped more carefully. Here we check if the line of
 * sight intersects any of the polyobj segs that face the camera.
 *
 * @param ssecidx  Subsector index in which lumobjs will be clipped.
 */
void LO_ClipInSubsectorBySight(uint ssecidx);

/**
 * Iterate over all luminous objects within the specified origin range, making
 * a callback for each visited. Iteration ends when all selected luminous objects
 * have been visited or a callback returns non-zero.
 *
 * @param subsector  The subsector in which the origin resides.
 * @param x  X coordinate of the origin (must be within @a subsector).
 * @param y  Y coordinate of the origin (must be within @a subsector).
 * @param radius  Radius of the range around the origin point.
 * @param callback  Callback to make for each object.
 * @param paramaters  Data to pass to the callback.
 *
 * @return  @c 0 iff iteration completed wholly.
 */
int LO_LumobjsRadiusIterator2(subsector_t* subsector, float x, float y, float radius,
    int (*callback) (const lumobj_t* lum, float distance, void* paramaters), void* paramaters);
int LO_LumobjsRadiusIterator(subsector_t* subsector, float x, float y, float radius,
    int (*callback) (const lumobj_t* lum, float distance, void* paramaters)); /* paramaters = NULL */

void LO_DrawLumobjs(void);

#endif /* LIBDENG_REFRESH_LUMINOUS_H */
