/**\file r_world.h
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
 * World Setup and Refresh.
 */

#ifndef LIBDENG_REFRESH_WORLD_H
#define LIBDENG_REFRESH_WORLD_H

#include "r_data.h"

// Used for vertex sector owners, side line owners and reverb subsectors.
typedef struct ownernode_s {
    void*           data;
    struct ownernode_s* next;
} ownernode_t;

typedef struct {
    ownernode_t*    head;
    uint            count;
} ownerlist_t;

typedef struct skyfix_s {
    float           height;
} skyfix_t;

extern float rendSkyLight; // cvar
extern byte rendSkyLightAuto; // cvar
extern float rendLightWallAngle;
extern byte rendLightWallAngleSmooth;
extern boolean ddMapSetup;
extern skyfix_t skyFix[2]; // [floor, ceiling]

// Sky flags.
#define SIF_DRAW_SPHERE     0x1 // Always draw the sky sphere.

/**
 * Called by the game at various points in the map setup process.
 */
void R_SetupMap(int mode, int flags);

void            R_InitLinks(gamemap_t* map);
void            R_PolygonizeMap(gamemap_t* map);
void            R_SetupFog(float start, float end, float density, float* rgb);
void            R_SetupFogDefaults(void);

/**
 * Sector light color may be affected by the sky light color.
 */
const float* R_GetSectorLightColor(const sector_t* sector);

float           R_DistAttenuateLightLevel(float distToViewer, float lightLevel);

/**
 * The DOOM lighting model applies a light level delta to everything when
 * e.g. the player shoots.
 *
 * @return  Calculated delta.
 */
float R_ExtraLightDelta(void);

/**
 * @return  @c > 0 if @a lightlevel passes the min max limit condition.
 */
float R_CheckSectorLight(float lightlevel, float min, float max);

/**
 * Will the specified surface be added to the sky mask?
 *
 * @param suf  Ptr to the surface to test.
 * @return boolean  @c true, iff the surface will be masked.
 */
boolean R_IsSkySurface(const surface_t* suf);

boolean R_SectorContainsSkySurfaces(const sector_t* sec);

void R_UpdatePlanes(void);
void R_ClearSectorFlags(void);
void R_InitSkyFix(void);
void R_MapInitSurfaceLists(void);

void            R_UpdateSkyFixForSec(const sector_t* sec);
void            R_OrderVertices(const linedef_t* line, const sector_t* sector,
                                vertex_t* verts[2]);
boolean         R_FindBottomTop(segsection_t section, float segOffset,
                                const surface_t* suf,
                                const plane_t* ffloor, const plane_t* fceil,
                                const plane_t* bfloor, const plane_t* bceil,
                                boolean unpegBottom, boolean unpegTop,
                                boolean stretchMiddle, boolean isSelfRef,
                                float* bottom, float* top, float texOffset[2]);
plane_t*        R_NewPlaneForSector(sector_t* sec);
void            R_DestroyPlaneOfSector(uint id, sector_t* sec);

surfacedecor_t* R_CreateSurfaceDecoration(surface_t* suf);
void            R_ClearSurfaceDecorations(surface_t* suf);

void            R_UpdateWatchedPlanes(watchedplanelist_t* wpl);
void            R_InterpolateWatchedPlanes(watchedplanelist_t* wpl,
                                           boolean resetNextViewer);
void            R_AddWatchedPlane(watchedplanelist_t* wpl, plane_t* pln);
boolean         R_RemoveWatchedPlane(watchedplanelist_t* wpl,
                                     const plane_t* pln);

void            R_UpdateMovingSurfaces(void);
void            R_InterpolateMovingSurfaces(boolean resetNextViewer);

/**
 * Adds the surface to the given surface list.
 *
 * @param sl  The surface list to add the surface to.
 * @param suf  The surface to add to the list.
 */
void R_SurfaceListAdd(surfacelist_t* sl, surface_t* suf);
boolean R_SurfaceListRemove(surfacelist_t* sl, const surface_t* suf);
void R_SurfaceListClear(surfacelist_t* sl);

/**
 * Iterate the list of surfaces making a callback for each.
 *
 * @param callback  The callback to make. Iteration will continue until
 *      a callback returns a zero value.
 * @param context  Is passed to the callback function.
 */
boolean R_SurfaceListIterate(surfacelist_t* sl, boolean (*callback) (surface_t* suf, void*), void* context);

void            R_MarkDependantSurfacesForDecorationUpdate(plane_t* pln);

/**
 * To be called in response to a Material property changing which may
 * require updating any map surfaces which are presently using it.
 */
void R_UpdateMapSurfacesOnMaterialChange(material_t* material);

/// @return  @c true= @a plane is non-glowing (i.e. not glowing or a sky).
boolean R_IsGlowingPlane(const plane_t* plane);

float R_GlowStrength(const plane_t* pln);

lineowner_t*    R_GetVtxLineOwner(const vertex_t* vtx, const linedef_t* line);
linedef_t*      R_FindLineNeighbor(const sector_t* sector,
                                   const linedef_t* line,
                                   const lineowner_t* own,
                                   boolean antiClockwise, binangle_t* diff);
linedef_t*      R_FindSolidLineNeighbor(const sector_t* sector,
                                        const linedef_t* line,
                                        const lineowner_t* own,
                                        boolean antiClockwise,
                                        binangle_t* diff);
linedef_t*      R_FindLineBackNeighbor(const sector_t* sector,
                                       const linedef_t* line,
                                       const lineowner_t* own,
                                       boolean antiClockwise,
                                       binangle_t* diff);
#endif /* LIBDENG_REFRESH_WORLD_H */
