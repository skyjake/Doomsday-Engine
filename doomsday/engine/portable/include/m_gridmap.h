/**
 * @file gridmap.h
 * Gridmap. An abstract class designed for mapping objects into a two
 * dimensional spatial index.
 *
 * Gridmap's implementation allows that the whole space is indexable
 * however cells within it need not be populated. Therefore Gridmap may
 * be considered a "sparse" structure as it allows the user to construct
 * the space piece-wise or, leave it deliberately incomplete.
 *
 * @ingroup data
 *
 * @authors Copyright &copy; 2009-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_DATA_GRIDMAP_H
#define LIBDENG_DATA_GRIDMAP_H

#include "dd_types.h"

/**
 * GridmapBlock. Handy POD structure for defining a rectangular range
 * of cells, i.e., a "cell block".
 */
typedef struct gridmapblock_s {
    union {
        struct {
            uint minX;
            uint minY;
            uint maxX;
            uint maxY;
        };
        struct {
            uint min[2];
            uint max[2];
        };
        struct {
            uint box[4];
        };
    };
} GridmapBlock;

struct gridmap_s; // The Gridmap instance (opaque).
typedef struct gridmap_s Gridmap;

/**
 * Create a new initial Gridmap (empty).
 *
 * @param width        X dimension in cells.
 * @param height       Y dimension in cells.
 * @param sizeOfCell   Amount of memory to be allocated for the user data associated with each cell.
 */
Gridmap* Gridmap_New(uint width, uint height, size_t sizeOfCell, int zoneTag);

void Gridmap_Delete(Gridmap* gridmap);

/// @return  Width of the Gridmap in cells.
uint Gridmap_Width(const Gridmap* gridmap);

/// @return  Height of the Gridmap in cells.
uint Gridmap_Height(const Gridmap* gridmap);

/**
 * Retrieve the dimensions of the Gridmap in cells.
 * @param widthHeight   Dimensions written here.
 */
void Gridmap_Size(const Gridmap* gridmap, uint widthHeight[2]);

/**
 * Retrieve the user data associated with the identified cell.
 *
 * @param coords        XY coordinates of the cell whose data to retrieve.
 * @param alloc         @c true= we should allocate new user data if not already present
 *                      for the selected cell.
 *
 * @return  User data for the identified cell else @c NULL if an invalid reference or no
 *          there is no data present (and not allocating).
 */
void* Gridmap_Cell(Gridmap* gridmap, uint const coords[2], boolean alloc);

/**
 * Same as Gridmap::Cell except cell coordinates are expressed with @a x and @a y arguments.
 */
void* Gridmap_CellXY(Gridmap* gridmap, uint x, uint y, boolean alloc);

/**
 * Iteration.
 */

typedef int (C_DECL *Gridmap_IterateCallback) (void* cellData, void* paramaters);

/**
 * Iterate over populated cells in the Gridmap making a callback for each. Iteration ends
 * when all cells have been visited or @a callback returns non-zero.
 *
 * @param callback       Callback function ptr.
 * @param paramaters     Passed to the callback.
 *
 * @return  @c 0 iff iteration completed wholly.
 */
int Gridmap_Iterate2(Gridmap* gridmap, Gridmap_IterateCallback callback, void* paramaters);
int Gridmap_Iterate(Gridmap* gridmap, Gridmap_IterateCallback callback); /*paramaters=NULL*/

/**
 * Iterate over a block of populated cells in the Gridmap making a callback for each.
 * Iteration ends when all selected cells have been visited or @a callback returns non-zero.
 *
 * @param min            Minimal coordinates for the top left cell.
 * @param max            Maximal coordinates for the bottom right cell.
 * @param callback       Callback function ptr.
 * @param paramaters     Passed to the callback.
 *
 * @return  @c 0 iff iteration completed wholly.
 */
int Gridmap_BlockIterate2(Gridmap* gridmap, const GridmapBlock* block,
    Gridmap_IterateCallback callback, void* paramates);
int Gridmap_BlockIterate(Gridmap* gridmap, const GridmapBlock* block,
    Gridmap_IterateCallback callback); /*paramaters=NULL*/

/**
 * Same as Gridmap::BlockIterate except cell block coordinates are expressed with
 * independent X and Y coordinate arguments. For convenience.
 */
int Gridmap_BlockXYIterate2(Gridmap* gridmap, uint minX, uint minY, uint maxX, uint maxY,
    Gridmap_IterateCallback callback, void* paramaters);
int Gridmap_BlockXYIterate(Gridmap* gridmap, uint minX, uint minY, uint maxX, uint maxY,
    Gridmap_IterateCallback callback); /*paramaters=NULL*/

/**
 * Clip the cell coordinates in @a block vs the dimensions of this Gridmap so that they
 * are inside the boundary this defines.
 *
 * @param block           Block coordinates to be clipped.
 *
 * @return  @c true iff the block coordinates were changed.
 */
boolean Gridmap_ClipBlock(Gridmap* gm, GridmapBlock* block);

/**
 * Static utility methods:
 */

/**
 * Initialize @a block using the specified coordinates.
 */
void Gridmap_InitBlock(GridmapBlock* block, uint minX, uint minY, uint maxX, uint maxY);

/**
 * Render a visual for this Gridmap to assist in debugging (etc...).
 *
 * This visualizer assumes that the caller has already configured the GL render state
 * (projection matrices, scale, etc...) as desired prior to calling. This function
 * guarantees to restore the previous GL state if any changes are made to it.
 *
 * @note Internally this visual uses fixed unit dimensions [1x1] for cells, therefore
 *       the caller should scale the appropriate matrix to scale this visual as desired.
 */
void Gridmap_Debug(Gridmap* gridmap);

#endif /// LIBDENG_DATA_GRIDMAP_H
