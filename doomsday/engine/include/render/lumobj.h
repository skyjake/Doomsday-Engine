/**
 * @file lumobj.h Luminous Object Management
 * @ingroup render
 *
 * @author Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_RENDER_LUMINOUS_H
#define LIBDENG_RENDER_LUMINOUS_H

#include "dd_types.h"
#include "color.h"
#include <de/vector1.h>
#include "map/p_mapdata.h"
#include "resource/r_data.h"

// Luminous object types.
typedef enum {
    LT_OMNI,  ///< Omni (spherical) light.
    LT_PLANE  ///< Planar light.
} lumtype_t;

// Helper macros for accessing lum data.
#define LUM_OMNI(x)         (&((x)->data.omni))
#define LUM_PLANE(x)        (&((x)->data.plane))

typedef struct lumobj_s {
    lumtype_t type;
    coord_t origin[3]; // Center of the obj.
    BspLeaf *bspLeaf;
    coord_t maxDistance;
    void *decorSource; // decorsource_t ptr, else @c NULL.

    union lumobj_data_u {
        struct lumobj_omni_s {
            float color[3];
            coord_t radius; // Radius for this omnilight source.
            coord_t zOff; // Offset to center from pos[VZ].
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

/**
 * Dynlight stores a luminous object => surface projection.
 */
typedef struct {
    DGLuint texture;
    float s[2], t[2];
    ColorRawf color;
} dynlight_t;

DENG_EXTERN_C boolean loInited;

DENG_EXTERN_C uint loMaxLumobjs;
DENG_EXTERN_C int loMaxRadius;
DENG_EXTERN_C float loRadiusFactor;
DENG_EXTERN_C byte rendInfoLums;
DENG_EXTERN_C int useMobjAutoLights;

#ifdef __cplusplus
extern "C" {
#endif

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
 * Construct a new lumobj and link it into @a bspLeaf.
 * @return  Logical index (name) for referencing the new lumobj.
 */
uint LO_NewLuminous(lumtype_t type, BspLeaf *bspLeaf);

/// @return  Lumobj associated with logical index @a idx else @c NULL.
lumobj_t *LO_GetLuminous(uint idx);

/// @return  Logical index associated with lumobj @a lum.
uint LO_ToIndex(lumobj_t const *lum);

/// @return  @c true if the lumobj is clipped for the viewer.
boolean LO_IsClipped(uint idx, int i);

/// @return  @c true if the lumobj is hidden for the viewer.
boolean LO_IsHidden(uint idx, int i);

/// @return  Approximated distance between the lumobj and the viewer.
coord_t LO_DistanceToViewer(uint idx, int i);

/**
 * Calculate a distance attentuation factor for a lumobj.
 *
 * @param idx           Logical index associated with the lumobj.
 * @param distance      Distance between the lumobj and the viewer.
 *
 * @return  Attentuation factor [0..1]
 */
float LO_AttenuationFactor(uint idx, coord_t distance);

/**
 * Clip lumobj, omni lights in the given BspLeaf.
 *
 * @param bspLeafIdx    BspLeaf index in which lights will be clipped.
 */
void LO_ClipInBspLeaf(uint bspLeafIdx);

/**
 * In the situation where a BSP leaf contains both lumobjs and a polyobj,
 * the lumobjs must be clipped more carefully. Here we check if the line of
 * sight intersects any of the polyobj hedges that face the camera.
 *
 * @param bspLeafIdx    BspLeaf index in which lumobjs will be clipped.
 */
void LO_ClipInBspLeafBySight(uint bspLeafIdx);

/**
 * Iterate over all luminous objects within the specified origin range, making
 * a callback for each visited. Iteration ends when all selected luminous objects
 * have been visited or a callback returns non-zero.
 *
 * @param bspLeaf       BspLeaf in which the origin resides.
 * @param x             X coordinate of the origin (must be within @a bspLeaf).
 * @param y             Y coordinate of the origin (must be within @a bspLeaf).
 * @param radius        Radius of the range around the origin point.
 * @param callback      Callback to make for each object.
 * @param paramaters    Data to pass to the callback.
 *
 * @return  @c 0 iff iteration completed wholly.
 */
int LO_LumobjsRadiusIterator2(BspLeaf *bspLeaf, coord_t x, coord_t y, coord_t radius, int (*callback) (lumobj_t const *lum, coord_t distance, void *paramaters), void* paramaters);
int LO_LumobjsRadiusIterator(BspLeaf *bspLeaf, coord_t x, coord_t y, coord_t radius, int (*callback) (lumobj_t const *lum, coord_t distance, void *paramaters)/*, paramaters = 0 */);

/**
 * @defgroup projectLightFlags  Flags for LO_ProjectToSurface
 * @ingroup flags
 */
///@{
#define PLF_SORT_LUMINOSITY_DESC    0x1 ///< Sort by descending luminosity, brightest to dullest.
#define PLF_NO_PLANE                0x2 ///< Surface is not lit by planar lights.
#define PLF_TEX_FLOOR               0x4 ///< Prefer the "floor" slot when picking textures.
#define PLF_TEX_CEILING             0x8 ///< Prefer the "ceiling" slot when picking textures.
///@}

/**
 * Project all lights affecting the given quad (world space), calculate
 * coordinates (in texture space) then store into a new list of projections.
 *
 * @pre The coordinates of the given quad must be contained wholly within
 * the BSP leaf specified. This is due to an optimization within the lumobj
 * management which separates them according to their position in the BSP.
 *
 * @param flags         @ref projectLightFlags
 * @param bspLeaf       BspLeaf within which the quad wholly resides.
 * @param blendFactor   Multiplied with projection alpha.
 * @param topLeft       Top left coordinates of the surface being projected to.
 * @param bottomRight   Bottom right coordinates of the surface being projected to.
 * @param topLeft       Top left coordinates of the surface being projected to.
 * @param bottomRight   Bottom right coordinates of the surface being projected to.
 * @param tangent       Normalized tangent of the surface being projected to.
 * @param bitangent     Normalized bitangent of the surface being projected to.
 * @param normal        Normalized normal of the surface being projected to.
 *
 * @return  Projection list identifier if surface is lit else @c 0.
 */
uint LO_ProjectToSurface(int flags, BspLeaf *bspLeaf, float blendFactor,
    vec3d_t topLeft, vec3d_t bottomRight, vec3f_t tangent, vec3f_t bitangent, vec3f_t normal);

/**
 * Iterate over projections in the identified surface-projection list, making
 * a callback for each visited. Iteration ends when all selected projections
 * have been visited or a callback returns non-zero.
 *
 * @param listIdx       Unique identifier of the list to process.
 * @param callback      Callback to make for each visited projection.
 * @param paramaters    Passed to the callback.
 *
 * @return  @c 0 iff iteration completed wholly.
 */
int LO_IterateProjections2(uint listIdx, int (*callback) (dynlight_t const *, void *), void *paramaters);
int LO_IterateProjections(uint listIdx, int (*callback) (dynlight_t const *, void*)/*, paramaters = 0*/);

void LO_DrawLumobjs(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /// LIBDENG_RENDER_LUMINOUS_H
