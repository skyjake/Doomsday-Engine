/**
 * @file blockmap.h
 * Blockmap. @ingroup map
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_MAP_BLOCKMAP_H
#define LIBDENG_MAP_BLOCKMAP_H

#include "dd_types.h"
#include "m_vector.h"
#include "p_mapdata.h"
#include "gridmap.h"

struct blockmap_s; // The Blockmap instance (opaque).
typedef struct blockmap_s Blockmap;

Blockmap* Blockmap_New(const pvec2_t min, const pvec2_t max, uint cellWidth, uint cellHeight);

/**
 * @return  "Origin" map space point for the Blockmap (minimal [x,y]).
 */
const pvec2_t Blockmap_Origin(Blockmap* blockmap);

/**
 * Retrieve the extremal map space points covered by the Blockmap.
 */
const AABoxf* Blockmap_Bounds(Blockmap* blockmap);

/// @return  Width of the Blockmap in cells.
uint Blockmap_Width(Blockmap* blockmap);

/// @return  Height of the Blockmap in cells.
uint Blockmap_Height(Blockmap* blockmap);

/**
 * Retrieve the size of the Blockmap in cells.
 *
 * @param widthHeight  Size of the Blockmap [width,height] written here.
 */
void Blockmap_Size(Blockmap* blockmap, uint widthHeight[2]);

/// @return  Width of a Blockmap cell in map space units.
float Blockmap_CellWidth(Blockmap* blockmap);

/// @return  Height of a Blockmap cell in map space units.
float Blockmap_CellHeight(Blockmap* blockmap);

/// @return  Size [width,height] of a Blockmap cell in map space units.
const pvec2_t Blockmap_CellSize(Blockmap* blockmap);

/**
 * Given map space X coordinate @a x, return the corresponding cell coordinate.
 * If @a x is outside the Blockmap it will be clamped to the nearest edge on
 * the X axis.
 */
uint Blockmap_CellX(Blockmap* blockmap, float x);

/**
 * Given map space Y coordinate @a y, return the corresponding cell coordinate.
 * If @a y is outside the Blockmap it will be clamped to the nearest edge on
 * the Y axis.
 *
 * @param outY  Blockmap cell X coordinate written here.
 * @param y  Map space X coordinate to be translated.
 * @return  @c true iff clamping was necessary.
 */
uint Blockmap_CellY(Blockmap* blockmap, float y);

/**
 * Same as @a Blockmap::CellX with alternative semantics for when the caller
 * needs to know if the coordinate specified was inside/outside the Blockmap.
 */
boolean Blockmap_ClipCellX(Blockmap* bm, uint* outX, float x);

/**
 * Same as @a Blockmap::CellY with alternative semantics for when the caller
 * needs to know if the coordinate specified was inside/outside the Blockmap.
 *
 * @param outY  Blockmap cell Y coordinate written here.
 * @param y  Map space Y coordinate to be translated.
 * @return  @c true iff clamping was necessary.
 */
boolean Blockmap_ClipCellY(Blockmap* bm, uint* outY, float y);

/**
 * Given map space XY coordinates @a pos, output the Blockmap cell[x, y] it
 * resides in. If @a pos is outside the Blockmap it will be clamped to the
 * nearest edge on one or more axes as necessary.
 *
 * @return  @c true iff clamping was necessary.
 */
boolean Blockmap_CellCoords(Blockmap* blockmap, uint coords[2], float const pos[2]);

/**
 * Given map space box XY coordinates @a box, output the blockmap cells[x, y]
 * they reside in. If any point defined by @a box lies outside the blockmap
 * it will be clamped to the nearest edge on one or more axes as necessary.
 *
 * @return  @c true iff Clamping was necessary.
 */
boolean Blockmap_CellBlockCoords(Blockmap* blockmap, GridmapBlock* blockCoords, const AABoxf* box);

boolean Blockmap_CreateCellAndLinkObject(Blockmap* blockmap, uint const coords[2], void* object);

boolean Blockmap_CreateCellAndLinkObjectXY(Blockmap* blockmap, uint x, uint y, void* object);

boolean Blockmap_UnlinkObjectInCell(Blockmap* blockmap, uint const coords[2], void* object);

boolean Blockmap_UnlinkObjectInCellXY(Blockmap* blockmap, uint x, uint y, void* object);

void Blockmap_UnlinkObjectInCellBlock(Blockmap* blockmap, const GridmapBlock* blockCoords, void* object);

int Blockmap_IterateCellObjects(Blockmap* blockmap, uint const coords[2],
    int (*callback) (void* object, void* context), void* context);

int Blockmap_IterateCellBlockObjects(Blockmap* blockmap, const GridmapBlock* blockCoords,
    int (*callback) (void* object, void* context), void* context);

#endif /// LIBDENG_MAP_BLOCKMAP_H
