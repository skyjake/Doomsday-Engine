/** @file r_world.h World Setup and Refresh.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBDENG_MAP_WORLD_H
#define LIBDENG_MAP_WORLD_H

#include <de/Observers>
#include <de/Vector>

#include "resource/r_data.h"
#include "map/line.h"
#include "map/plane.h"
#include "map/sector.h"
#include "map/sidedef.h"
#include "map/vertex.h"

/// @todo The MapChange audience belongs in a class.
DENG2_DECLARE_AUDIENCE(MapChange, void currentMapChanged())
DENG2_EXTERN_AUDIENCE(MapChange)

extern float rendSkyLight; // cvar
extern byte rendSkyLightAuto; // cvar
extern float rendLightWallAngle;
extern byte rendLightWallAngleSmooth;
extern boolean ddMapSetup;
extern boolean firstFrameAfterLoad;

// Sky flags.
#define SIF_DRAW_SPHERE     0x1 ///< Always draw the sky sphere.

/**
 * Sector light color may be affected by the sky light color.
 */
de::Vector3f const &R_GetSectorLightColor(Sector const &sector);

float R_DistAttenuateLightLevel(float distToViewer, float lightLevel);

/**
 * The DOOM lighting model applies a light level delta to everything when
 * e.g. the player shoots.
 *
 * @return  Calculated delta.
 */
float R_ExtraLightDelta();

/**
 * @return  @c > 0 if @a lightlevel passes the min max limit condition.
 */
float R_CheckSectorLight(float lightlevel, float min, float max);

boolean R_SectorContainsSkySurfaces(Sector const *sec);

void R_ClearSectorFlags();

void R_UpdateMissingMaterialsForLinesOfSector(Sector const &sec);

/**
 * Returns pointers to the line's vertices in such a fashion that @c verts[0]
 * is the leftmost vertex and @c verts[1] is the rightmost vertex, when the
 * @a line lies at the edge of @a sector.
 */
void R_OrderVertices(Line *line, Sector const *sector, Vertex *verts[2]);

/**
 * Determine the map space Z coordinates of a wall section.
 *
 * @param section       Identifier of the section to determine coordinates for.
 * @param lineFlags     @ref ldefFlags.
 * @param frontSec      Sector in front of the wall.
 * @param backSec       Sector behind the wall. Can be @c NULL
 * @param front         Front line side. Can be @c NULL.  @todo Refactor away
 * @param back          Back line side. Can be @c NULL.  @todo Refactor away
 *
 * Return values:
 * @param low           Z map space coordinate at the bottom of the wall section.
 * @param hi            Z map space coordinate at the top of the wall section.
 * @param matOffset     Surface space material coordinate offset. Can be @c NULL
 *
 * @return  @c true iff the determined wall section height is @c >0
 */
boolean R_FindBottomTop(SideDefSection section, int lineFlags,
    Sector const *frontSec, Sector const *backSec,
    Line::Side const *front, Line::Side const *back,
    coord_t *low, coord_t *hi, pvec2f_t matOffset = 0);

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
coord_t R_OpenRange(Sector const *frontSec, Sector const *backSec, coord_t *retBottom, coord_t *retTop);

/**
 * @copydoc R_OpenRange()
 *
 * @param line      Line to find the open range for.
 * @param side      Logical side of the line to find the open range for.
 * @param bottom    Bottom Z height is written here. Can be @c NULL.
 * @param top       Top Z height is written here. Can be @c NULL.
 *
 * @return Height of the open range.
 */
coord_t R_OpenRange(Line const &line, int side, coord_t *bottom, coord_t *top);

/**
 * Same as @ref R_OpenRange() but works with the "visual" (i.e., smoothed) plane
 * height coordinates rather than the "sharp" coordinates.
 *
 * @param frontSec  Sector on the front side.
 * @param backSec   Sector on the back side. Can be @c NULL.
 * @param bottom    Bottom Z height is written here. Can be @c NULL.
 * @param top       Top Z height is written here. Can be @c NULL.
 *
 * @return Height of the open range.
 */
coord_t R_VisOpenRange(Sector const *frontSec, Sector const *backSec, coord_t *retBottom, coord_t *retTop);

/**
 * @copydoc R_VisOpenRange()
 *
 * @param line      Line to find the open range for.
 * @param side      Logical side of the line to find the open range for.
 * @param bottom    Bottom Z height is written here. Can be @c NULL.
 * @param top       Top Z height is written here. Can be @c NULL.
 *
 * @return Height of the open range.
 */
coord_t R_VisOpenRange(Line const &line, int side, coord_t *bottom, coord_t *top);

#ifdef __CLIENT__
/**
 * @param lineFlags     @ref ldefFlags.
 * @param frontSec      Sector in front of the wall.
 * @param backSec       Sector behind the wall. Can be @c NULL
 * @param front         Front line side. Can be @c NULL. @todo Refactor away
 * @param back          Back line side. Can be @c NULL.  @todo Refactor away
 * @param ignoreOpacity @c true= material opacity should be ignored.
 *
 * @return  @c true iff SideDef @a frontDef has a "middle" Material which completely
 *     covers the open range defined by sectors @a frontSec and @a backSec.
 */
boolean R_MiddleMaterialCoversOpening(int lineFlags, Sector const *frontSec,
    Sector const *backSec, Line::Side const *front, Line::Side const *back,
    boolean ignoreOpacity = true);

/**
 * Same as @ref R_MiddleMaterialCoversOpening except all arguments are derived from
 * the specified line @a line.
 *
 * @note Anything calling this is likely working at the wrong level (should work with
 *       hedges instead).
 */
boolean R_MiddleMaterialCoversLineOpening(Line const *line, int side, boolean ignoreOpacity);
#endif // __CLIENT__

void R_UpdateSector(Sector &sector, bool forceUpdate = false);

/// @return  Current glow strength for the plane.
float R_GlowStrength(Plane const *pln);

LineOwner *R_GetVtxLineOwner(Vertex const *vtx, Line const *line);

#ifdef __CLIENT__
/**
 * A neighbour is a line that shares a vertex with 'line', and faces the
 * specified sector.
 */
Line *R_FindLineNeighbor(Sector const *sector, Line const *line,
    LineOwner const *own, boolean antiClockwise, binangle_t *diff);

Line *R_FindSolidLineNeighbor(Sector const *sector, Line const *line,
    LineOwner const *own, boolean antiClockwise, binangle_t *diff);

/**
 * A line's align neighbor is a line that shares a vertex with 'line' and
 * whos orientation is aligned with it (thus, making it unnecessary to have
 * a shadow between them. In practice, they would be considered a single,
 * long sidedef by the shadow generator).
 */
Line *R_FindLineAlignNeighbor(Sector const *sec, Line const *line,
    LineOwner const *own, boolean antiClockwise, int alignment);

/**
 * Find a backneighbour for the given line. They are the neighbouring line
 * in the backsector of the imediate line neighbor.
 */
Line *R_FindLineBackNeighbor(Sector const *sector, Line const *line,
    LineOwner const *own, boolean antiClockwise, binangle_t *diff);
#endif // __CLIENT__

/**
 * @defgroup skyCapFlags  Sky Cap Flags
 * @ingroup flags
 */
///@{
#define SKYCAP_LOWER                0x1
#define SKYCAP_UPPER                0x2
///@}

coord_t R_SkyCapZ(BspLeaf *bspLeaf, int skyCap);

#endif /* LIBDENG_MAP_WORLD_H */
