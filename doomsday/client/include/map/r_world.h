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
#include "Line"
#include "Plane"
#include "Sector"
#include "Vertex"

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

#ifdef __CLIENT__

float R_DistAttenuateLightLevel(float distToViewer, float lightLevel);

#endif // __CLIENT__

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
 * Determine the map space Z coordinates of a wall section.
 *
 * @param side          Line side to determine Z heights for.
 * @param section       Line::Side section to determine coordinates for.
 * @param frontSec      Sector in front of the wall.
 * @param backSec       Sector behind the wall. Can be @c NULL
 *
 * Return values:
 * @param bottom        Z map space coordinate at the bottom of the wall section. Can be @c 0.
 * @param top           Z map space coordinate at the top of the wall section. Can be @c 0.
 * @param materialOrigin  Surface space material coordinate offset. Can be @c 0.
 */
void R_SideSectionCoords(Line::Side const &side, int section, Sector const *frontSec,
    Sector const *backSec, coord_t *bottom = 0, coord_t *top = 0,
    de::Vector2f *materialOrigin = 0);

/**
 * Same as @ref R_SideSectionCoords() except that the sector arguments are taken from
 * the specified line @a side.
 */
inline void R_SideSectionCoords(Line::Side const &side, int section, coord_t *bottom = 0,
                                coord_t *top = 0, de::Vector2f *materialOrigin = 0)
{
    R_SideSectionCoords(side, section, side.sectorPtr(), side.back().sectorPtr(),
                        bottom, top, materialOrigin);
}

/**
 * Find the "sharp" Z coordinate range of the opening between sectors @a frontSec
 * and @a backSec. The open range is defined as the gap between foor and ceiling on
 * the front side clipped by the floor and ceiling planes on the back side (if present).
 *
 * @param side      Line side to find the open range for.
 * @param frontSec  Sector on the front side.
 * @param backSec   Sector on the back side. Can be @c 0.
 *
 * Return values:
 * @param bottom    Bottom Z height is written here. Can be @c 0.
 * @param top       Top Z height is written here. Can be @c 0.
 *
 * @return Height of the open range.
 */
coord_t R_OpenRange(Line::Side const &side, Sector const *frontSec, Sector const *backSec,
    coord_t *bottom = 0, coord_t *top = 0);

/**
 * Same as @ref R_OpenRange() except that the sector arguments are taken from the
 * specified line @a side.
 */
inline coord_t R_OpenRange(Line::Side const &side, coord_t *bottom = 0, coord_t *top = 0)
{
    return R_OpenRange(side, side.sectorPtr(), side.back().sectorPtr(), bottom, top);
}

/**
 * Same as @ref R_OpenRange() but works with the "visual" (i.e., smoothed) plane
 * height coordinates rather than the "sharp" coordinates.
 *
 * @param side      Line side to find the open range for.
 * @param frontSec  Sector on the front side.
 * @param backSec   Sector on the back side. Can be @c 0.
 *
 * Return values:
 * @param bottom    Bottom Z height is written here. Can be @c 0.
 * @param top       Top Z height is written here. Can be @c 0.
 *
 * @return Height of the open range.
 */
coord_t R_VisOpenRange(Line::Side const &side, Sector const *frontSec, Sector const *backSec,
    coord_t *bottom = 0, coord_t *top = 0);

/**
 * Same as @ref R_VisOpenRange() except that the sector arguments are taken from
 * the specified line @a side.
 */
inline coord_t R_VisOpenRange(Line::Side const &side, coord_t *bottom = 0, coord_t *top = 0)
{
    return R_VisOpenRange(side, side.sectorPtr(), side.back().sectorPtr(), bottom, top);
}

#ifdef __CLIENT__
/**
 * @param side          Line side for which to determine covered opening status.
 * @param frontSec      Sector in front of the wall.
 * @param backSec       Sector behind the wall. Can be @c 0.
 * @param ignoreOpacity @c true= material opacity should be ignored.
 *
 * @return  @c true iff Line::Side @a front has a "middle" Material which completely
 *     covers the open range defined by sectors @a frontSec and @a backSec.
 *
 * @note Anything calling this is likely working at the wrong level (should work with
 * half-edges instead).
 */
bool R_MiddleMaterialCoversOpening(Line::Side const &side, Sector const *frontSec,
    Sector const *backSec, bool ignoreOpacity = false);

/**
 * Same as @ref R_MiddleMaterialCoversOpening() except that the sector arguments
 * are taken from the specified line @a side.
 */
inline bool R_MiddleMaterialCoversOpening(Line::Side const &side, bool ignoreOpacity = false)
{
    return R_MiddleMaterialCoversOpening(side, side.sectorPtr(), side.back().sectorPtr(),
                                         ignoreOpacity);
}

#endif // __CLIENT__

/**
 * @param side  Line::Side instance.
 * @param ignoreOpacity  @c true= do not consider Material opacity.
 *
 * @return  @c true if this side is considered "closed" (i.e., there is no opening
 * through which the relative back Sector can be seen). Tests consider all Planes
 * which interface with this and the "middle" Material used on the "this" side.
 */
bool R_SideBackClosed(Line::Side const &side, bool ignoreOpacity = true);

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
    LineOwner const *own, bool antiClockwise, binangle_t *diff = 0);

Line *R_FindSolidLineNeighbor(Sector const *sector, Line const *line,
    LineOwner const *own, bool antiClockwise, binangle_t *diff = 0);

/**
 * A line's align neighbor is a line that shares a vertex with 'line' and
 * whos orientation is aligned with it (thus, making it unnecessary to have
 * a shadow between them. In practice, they would be considered a single,
 * long side by the shadow generator).
 */
Line *R_FindLineAlignNeighbor(Sector const *sec, Line const *line,
    LineOwner const *own, bool antiClockwise, int alignment);

/**
 * Find a backneighbour for the given line. They are the neighbouring line
 * in the backsector of the imediate line neighbor.
 */
Line *R_FindLineBackNeighbor(Sector const *sector, Line const *line,
    LineOwner const *own, bool antiClockwise, binangle_t *diff = 0);
#endif // __CLIENT__

#endif /* LIBDENG_MAP_WORLD_H */
