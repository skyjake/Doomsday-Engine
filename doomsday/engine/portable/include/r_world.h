/**\file r_world.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifdef __cplusplus
extern "C" {
#endif

// Used for vertex sector owners, side line owners and reverb BSP leafs.
typedef struct ownernode_s {
    void*           data;
    struct ownernode_s* next;
} ownernode_t;

typedef struct {
    ownernode_t*    head;
    uint            count;
} ownerlist_t;

extern float rendSkyLight; // cvar
extern byte rendSkyLightAuto; // cvar
extern float rendLightWallAngle;
extern byte rendLightWallAngleSmooth;
extern boolean ddMapSetup;

// Sky flags.
#define SIF_DRAW_SPHERE     0x1 // Always draw the sky sphere.

/**
 * Called by the game at various points in the map setup process.
 */
void R_SetupMap(int mode, int flags);

void R_SetupFogDefaults(void);
void R_SetupFog(float start, float end, float density, float* rgb);

/**
 * Sector light color may be affected by the sky light color.
 */
const float* R_GetSectorLightColor(const Sector* sector);

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

boolean R_SectorContainsSkySurfaces(const Sector* sec);

void R_UpdatePlanes(void);
void R_ClearSectorFlags(void);
void R_MapInitSurfaceLists(void);

void R_OrderVertices(const LineDef* line, const Sector* sector, Vertex* verts[2]);

/**
 * Determine the map space Z coordinates of a wall section.
 *
 * @param section       Identifier of the section to determine coordinates for.
 * @param lineFlags     @ref ldefFlags.
 * @param frontSec      Sector in front of the wall.
 * @param backSec       Sector behind the wall. Can be @c NULL
 * @param frontDef      Definition for the front side. Can be @c NULL
 * @param backDef       Definition for the back side. Can be @c NULL
 *
 * Return values:
 * @param low           Z map space coordinate at the bottom of the wall section.
 * @param hi            Z map space coordinate at the top of the wall section.
 * @param matOffset     Surface space material coordinate offset. Can be @c NULL
 *
 * @return  @c true iff the determined wall section height is @c >0
 */
boolean R_FindBottomTop2(SideDefSection section, int lineFlags,
    Sector* frontSec, Sector* backSec, SideDef* frontDef, SideDef* backDef,
    coord_t* low, coord_t* hi, float matOffset[2]);
boolean R_FindBottomTop(SideDefSection section, int lineFlags,
    Sector* frontSec, Sector* backSec, SideDef* frontDef, SideDef* backDef,
    coord_t* low, coord_t* hi) /* matOffset = 0 */;

/**
 * Find the "sharp" Z coordinate range of the opening between sectors @a frontSec
 * and @a backSec. The open range is defined as the gap between foor and ceiling on
 * the front side clipped by the floor and ceiling planes on the back side (if present).
 *
 * @param frontSec  Sector on the front side.
 * @param backSec   Sector on the back side. Can be @c NULL.
 * @param bottom    Bottom Z height is written here. Can be @c NULL.
 * @param top       Top Z height is written here. Can be @c NULL.
 *
 * @return Height of the open range.
 */
coord_t R_OpenRange(Sector const* frontSec, Sector const* backSec, coord_t* retBottom, coord_t* retTop);

/// Same as @ref R_OpenRange() but works with the "visual" (i.e., smoothed) plane
/// height coordinates rather than the "sharp" coordinates.
coord_t R_VisOpenRange(Sector const* frontSec, Sector const* backSec, coord_t* retBottom, coord_t* retTop);

/**
 * @param lineFlags     @ref ldefFlags.
 * @param frontSec      Sector in front of the wall.
 * @param backSec       Sector behind the wall. Can be @c NULL
 * @param frontDef      Definition for the front side. Can be @c NULL
 * @param backDef       Definition for the back side. Can be @c NULL
 * @param ignoreOpacity @c true= material opacity should be ignored.
 *
 * @return  @c true iff SideDef @a frontDef has a "middle" Material which completely
 *     covers the open range defined by sectors @a frontSec and @a backSec.
 */
boolean R_MiddleMaterialCoversOpening(int lineFlags, Sector* frontSec, Sector* backSec,
    SideDef* frontDef, SideDef* backDef, boolean ignoreOpacity);

/**
 * Same as @ref R_MiddleMaterialCoversOpening except all arguments are derived from
 * the specified linedef @a line.
 *
 * @note Anything calling this is likely working at the wrong level (should work with
 *       hedges instead).
 */
boolean R_MiddleMaterialCoversLineOpening(LineDef* line, int side, boolean ignoreOpacity);

Plane*          R_NewPlaneForSector(Sector* sec);
void            R_DestroyPlaneOfSector(uint id, Sector* sec);

surfacedecor_t* R_CreateSurfaceDecoration(Surface* suf);
void            R_ClearSurfaceDecorations(Surface* suf);

void            R_UpdateTrackedPlanes(void);
void            R_InterpolateTrackedPlanes(boolean resetNextViewer);

void            R_AddTrackedPlane(planelist_t* plist, Plane* pln);
boolean         R_RemoveTrackedPlane(planelist_t* plist, const Plane* pln);

void            R_UpdateSurfaceScroll(void);
void            R_InterpolateSurfaceScroll(boolean resetNextViewer);

/**
 * Adds the surface to the given surface list.
 *
 * @param sl  The surface list to add the surface to.
 * @param suf  The surface to add to the list.
 */
void R_SurfaceListAdd(surfacelist_t* sl, Surface* suf);
boolean R_SurfaceListRemove(surfacelist_t* sl, const Surface* suf);
void R_SurfaceListClear(surfacelist_t* sl);

/**
 * Iterate the list of surfaces making a callback for each.
 *
 * @param callback  The callback to make. Iteration will continue until
 *      a callback returns a zero value.
 * @param context  Is passed to the callback function.
 */
boolean R_SurfaceListIterate(surfacelist_t* sl, boolean (*callback) (Surface* suf, void*), void* context);

void            R_MarkDependantSurfacesForDecorationUpdate(Plane* pln);

/**
 * To be called in response to a Material property changing which may
 * require updating any map surfaces which are presently using it.
 */
void R_UpdateMapSurfacesOnMaterialChange(material_t* material);

/// @return  @c true= @a plane is non-glowing (i.e. not glowing or a sky).
boolean R_IsGlowingPlane(const Plane* plane);

float R_GlowStrength(const Plane* pln);

lineowner_t* R_GetVtxLineOwner(const Vertex* vtx, const LineDef* line);

/**
 * A neighbour is a line that shares a vertex with 'line', and faces the
 * specified sector.
 */
LineDef* R_FindLineNeighbor(const Sector* sector, const LineDef* line,
    const lineowner_t* own, boolean antiClockwise, binangle_t* diff);

LineDef* R_FindSolidLineNeighbor(const Sector* sector, const LineDef* line,
    const lineowner_t* own, boolean antiClockwise, binangle_t* diff);

/**
 * A line's align neighbor is a line that shares a vertex with 'line' and
 * whos orientation is aligned with it (thus, making it unnecessary to have
 * a shadow between them. In practice, they would be considered a single,
 * long sidedef by the shadow generator).
 */
LineDef* R_FindLineAlignNeighbor(const Sector* sec, const LineDef* line,
    const lineowner_t* own, boolean antiClockwise, int alignment);

/**
 * Find a backneighbour for the given line. They are the neighbouring line
 * in the backsector of the imediate line neighbor.
 */
LineDef* R_FindLineBackNeighbor(const Sector* sector, const LineDef* line,
    const lineowner_t* own, boolean antiClockwise, binangle_t* diff);

/**
 * @defgroup skyCapFlags  Sky Cap Flags
 */
///@{
#define SKYCAP_LOWER                0x1
#define SKYCAP_UPPER                0x2
///@}

coord_t R_SkyCapZ(BspLeaf* bspLeaf, int skyCap);

#ifdef __cplusplus
}
#endif

#endif /* LIBDENG_REFRESH_WORLD_H */
