/**\file p_bmap.h
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
 * Map Blockmaps
 */

#ifndef LIBDENG_MAP_BLOCKMAP_H
#define LIBDENG_MAP_BLOCKMAP_H

#include "dd_types.h"
#include "m_vector.h"
#include "p_mapdata.h"
#include "m_gridmap.h"

/// Size of Blockmap blocks in map units. Must be an integer power of two.
#define MAPBLOCKUNITS               (128)

byte bmapShowDebug;
float bmapDebugSize;

/**
 * Construct an initial (empty) Mobj Blockmap for this map.
 *
 * @param min  Minimal coordinates for the map.
 * @param max  Maximal coordinates for the map.
 */
void Map_InitMobjBlockmap(gamemap_t* map, const_pvec2_t min, const_pvec2_t max);

void Map_LinkMobjInBlockmap(gamemap_t* map, mobj_t* mo);
boolean Map_UnlinkMobjInBlockmap(gamemap_t* map, mobj_t* mo);

int Map_IterateCellMobjs(gamemap_t* map, const uint coords[2],
    int (*callback) (struct mobj_s*, void*), void* paramaters);
int Map_IterateCellBlockMobjs(gamemap_t* map, const GridmapBlock* blockCoords,
    int (*callback) (struct mobj_s*, void*), void* paramaters);

/**
 * Construct an initial (empty) LineDef Blockmap for this map.
 *
 * @param min  Minimal coordinates for the map.
 * @param max  Maximal coordinates for the map.
 */
void Map_InitLineDefBlockmap(gamemap_t* map, const_pvec2_t min, const_pvec2_t max);

void Map_LinkLineDefInBlockmap(gamemap_t* map, linedef_t* lineDef);

int Map_IterateCellLineDefs(gamemap_t* map, const uint coords[2],
    int (*callback) (linedef_t*, void*), void* paramaters);
int Map_IterateCellBlockLineDefs(gamemap_t* map, const GridmapBlock* blockCoords,
    int (*callback) (linedef_t*, void*), void* paramaters);

/**
 * Construct an initial (empty) Subsector Blockmap for this map.
 *
 * @param min  Minimal coordinates for the map.
 * @param max  Maximal coordinates for the map.
 */
void Map_InitSubsectorBlockmap(gamemap_t* map, const_pvec2_t min, const_pvec2_t max);

void Map_LinkSubsectorInBlockmap(gamemap_t* map, subsector_t* subsector);

int Map_IterateCellSubsectors(gamemap_t* map, const uint coords[2],
    sector_t* sector, const AABoxf* box, int localValidCount,
    int (*callback) (subsector_t*, void*), void* paramaters);
int Map_IterateCellBlockSubsectors(gamemap_t* map, const GridmapBlock* blockCoords,
    sector_t* sector, const AABoxf* box, int localValidCount,
    int (*callback) (subsector_t*, void*), void* paramaters);

/**
 * Construct an initial (empty) Polyobj Blockmap for this map.
 *
 * @param min  Minimal coordinates for the map.
 * @param max  Maximal coordinates for the map.
 */
void Map_InitPolyobjBlockmap(gamemap_t* map, const_pvec2_t min, const_pvec2_t max);

void Map_LinkPolyobjInBlockmap(gamemap_t* map, polyobj_t* po);
void  Map_UnlinkPolyobjInBlockmap(gamemap_t* map, polyobj_t* po);

int Map_IterateCellPolyobjs(gamemap_t* map, const uint coords[2],
    int (*callback) (polyobj_t*, void*), void* paramaters);
int Map_IterateCellBlockPolyobjs(gamemap_t* map, const GridmapBlock* blockCoords,
    int (*callback) (polyobj_t*, void*), void* paramaters);

int Map_IterateCellPolyobjLineDefs(gamemap_t* map, const uint coords[2],
    int (*callback) (linedef_t*, void*), void* paramaters);
int Map_IterateCellBlockPolyobjLineDefs(gamemap_t* map, const GridmapBlock* blockCoords,
    int (*callback) (linedef_t*, void*), void* paramaters);

/**
 * Render the Blockmap debugging visual.
 * \todo Split this out into its own module.
 */
void Rend_BlockmapDebug(void);

#endif /* LIBDENG_MAP_BLOCKMAP_H */
