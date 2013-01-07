/**
 * @file blockmap.h
 * Blockmap. @ingroup map
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBDENG_MAP_BLOCKMAP_H
#define LIBDENG_MAP_BLOCKMAP_H

#include "dd_types.h"
#include <de/vector1.h>

/// @todo It should not be necessary to expose the Gridmap implementation.
#include "gridmap.h"

struct blockmap_s; // The Blockmap instance (opaque).
typedef struct blockmap_s Blockmap;

typedef GridmapCoord BlockmapCoord;
typedef GridmapCell BlockmapCell;
typedef const_GridmapCell const_BlockmapCell;

typedef GridmapCellBlock BlockmapCellBlock;

Blockmap* Blockmap_New(coord_t const min[2], coord_t const max[2], uint cellWidth, uint cellHeight);

/**
 * @param blockmap      Blockmap instance.
 * @return  "Origin" map space point for the Blockmap (minimal [x,y]).
 */
const pvec2d_t Blockmap_Origin(Blockmap* blockmap);

/**
 * Retrieve the extremal map space points covered by the Blockmap.
 *
 * @param blockmap      Blockmap instance.
 */
const AABoxd* Blockmap_Bounds(Blockmap* blockmap);

/**
 * @param blockmap      Blockmap instance.
 * @return  Width of the Blockmap in cells.
 */
BlockmapCoord Blockmap_Width(Blockmap* blockmap);

/**
 * @param blockmap  Blockmap instance.
 * @return  Height of the Blockmap in cells.
 */
BlockmapCoord Blockmap_Height(Blockmap* blockmap);

/**
 * Retrieve the size of the Blockmap in cells.
 *
 * @param blockmap     Blockmap instance.
 * @param widthHeight  Size of the Blockmap [width,height] written here.
 */
void Blockmap_Size(Blockmap* blockmap, BlockmapCoord widthHeight[2]);

/**
 * @param blockmap      Blockmap instance.
 * @return  Width of a Blockmap cell in map space units.
 */
coord_t Blockmap_CellWidth(Blockmap* blockmap);

/**
 * @param blockmap      Blockmap instance.
 * @return  Height of a Blockmap cell in map space units.
 */
coord_t Blockmap_CellHeight(Blockmap* blockmap);

/**
 * @param blockmap      Blockmap instance.
 * @return  Size [width,height] of a Blockmap cell in map space units.
 */
const pvec2d_t Blockmap_CellSize(Blockmap* blockmap);

/**
 * Given map space X coordinate @a x, return the corresponding cell coordinate.
 * If @a x is outside the Blockmap it will be clamped to the nearest edge on
 * the X axis.
 *
 * @param blockmap      Blockmap instance.
 * @param x             Map space X coordinate to be translated.
 *
 * @return  Translated Blockmap cell X coordinate.
 */
BlockmapCoord Blockmap_CellX(Blockmap* blockmap, coord_t x);

/**
 * Given map space Y coordinate @a y, return the corresponding cell coordinate.
 * If @a y is outside the Blockmap it will be clamped to the nearest edge on
 * the Y axis.
 *
 * @param blockmap      Blockmap instance.
 * @param y             Map space Y coordinate to be translated.
 *
 * @return  Translated Blockmap cell Y coordinate.
 */
BlockmapCoord Blockmap_CellY(Blockmap* blockmap, coord_t y);

/**
 * Same as @a Blockmap_CellX() with alternative semantics for when the caller
 * needs to know if the coordinate specified was inside/outside the Blockmap.
 */
boolean Blockmap_ClipCellX(Blockmap* blockmap, BlockmapCoord* outX, coord_t x);

/**
 * Same as @ref Blockmap_CellY() with alternative semantics for when the caller
 * needs to know if the coordinate specified was inside/outside the Blockmap.
 *
 * @param blockmap      Blockmap instance.
 * @param outY          Blockmap cell Y coordinate written here.
 * @param y             Map space Y coordinate to be translated.
 *
 * @return  @c true iff clamping was necessary.
 */
boolean Blockmap_ClipCellY(Blockmap* blockmap, BlockmapCoord* outY, coord_t y);

/**
 * Given map space XY coordinates @a pos, output the Blockmap cell[x, y] it
 * resides in. If @a pos is outside the Blockmap it will be clamped to the
 * nearest edge on one or more axes as necessary.
 *
 * @param blockmap      Blockmap instance.
 * @param cell          Blockmap cell coordinates will be written here.
 * @param pos           Map space coordinates to translate.
 *
 * @return  @c true iff clamping was necessary.
 */
boolean Blockmap_Cell(Blockmap* blockmap, BlockmapCell cell, coord_t const pos[2]);

/**
 * Given map space box XY coordinates @a box, output the blockmap cells[x, y]
 * they reside in. If any point defined by @a box lies outside the blockmap
 * it will be clamped to the nearest edge on one or more axes as necessary.
 *
 * @param blockmap      Blockmap instance.
 * @param cellBlock     Blockmap cell coordinates will be written here.
 * @param box           Map space coordinates to translate.
 *
 * @return  @c true iff Clamping was necessary.
 */
boolean Blockmap_CellBlock(Blockmap* blockmap, BlockmapCellBlock* cellBlock, const AABoxd* box);

/**
 * Retrieve the number objects linked in the specified @a cell.
 *
 * @param blockmap      Blockmap instance.
 * @param cell          Blockmap cell to lookup.
 *
 * @return  Number of unique objects linked into the cell, or @c 0 if invalid.
 */
uint Blockmap_CellObjectCount(Blockmap* blockmap, const_BlockmapCell cell);
uint Blockmap_CellXYObjectCount(Blockmap* blockmap, BlockmapCoord x, BlockmapCoord y);

boolean Blockmap_CreateCellAndLinkObject(Blockmap* blockmap, const_BlockmapCell cell, void* object);

boolean Blockmap_CreateCellAndLinkObjectXY(Blockmap* blockmap, BlockmapCoord x, BlockmapCoord y, void* object);

boolean Blockmap_UnlinkObjectInCell(Blockmap* blockmap, const_BlockmapCell cell, void* object);

boolean Blockmap_UnlinkObjectInCellXY(Blockmap* blockmap, BlockmapCoord x, BlockmapCoord y, void* object);

void Blockmap_UnlinkObjectInCellBlock(Blockmap* blockmap, const BlockmapCellBlock* blockCoords, void* object);

int Blockmap_IterateCellObjects(Blockmap* blockmap, const_BlockmapCell cell,
    int (*callback) (void* object, void* context), void* context);

int Blockmap_IterateCellBlockObjects(Blockmap* blockmap, const BlockmapCellBlock* blockCoords,
    int (*callback) (void* object, void* context), void* context);

/**
 * Retrieve an immutable pointer to the underlying Gridmap instance (mainly for
 * for debug purposes).
 */
const Gridmap* Blockmap_Gridmap(Blockmap* blockmap);

#endif /// LIBDENG_MAP_BLOCKMAP_H
