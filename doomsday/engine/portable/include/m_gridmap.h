/**\file m_gridmap.h
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
 * Generalized blockmap.
 */

#ifndef LIBDENG_DATA_GRIDMAP_H
#define LIBDENG_DATA_GRIDMAP_H

struct gridmap_s; // The Gridmap instance (opaque).
typedef struct gridmap_s Gridmap;

/**
 * Create a new Gridmap.
 *
 * @param width  X dimension of the grid.
 * @param height  Y dimension of the grid.
 * @param sizeOfBlock Amount of memory to be allocated for each block element.
 */
Gridmap* Gridmap_New(uint width, uint height, size_t sizeOfBlock, int zoneTag);

void Gridmap_Delete(Gridmap* gridmap);

/// @return  Width of the Gridmap in blocks.
uint Gridmap_Width(Gridmap* gridmap);

/// @return  Height of the Gridmap in blocks.
uint Gridmap_Height(Gridmap* gridmap);

/**
 * Retrieve the dimensions of the Gridmap in blocks.
 * @param widthHeight  Dimensions written here.
 */
void Gridmap_Size(Gridmap* gridmap, uint widthHeight[2]);

/**
 * Retrieve the user data associated with identified block.
 *
 * @param x  X coordinate of the block whose data to retrieve.
 * @param y  Y coordinate of the block whose data to retrieve.
 * @param alloc  @c true= we should allocate new user data if not present.
 *
 * @return  User data for the identified block else @c NULL if an invalid reference
 *     or no data present (and not allocating).
 */
void* Gridmap_Block(Gridmap* gridmap, uint x, uint y, boolean alloc);

/**
 * Iteration.
 */

typedef int (C_DECL *Gridmap_IterateCallback) (void* blockData, void* paramaters);

/**
 * Iterate over blocks in the Gridmap making a callback for each.
 * Iteration ends when all blocks have been visited or a callback
 * returns non-zero.
 *
 * @param callback  Callback function ptr.
 * @param paramaters  Passed to the callback.
 *
 * @return  @c 0 iff iteration completed wholly.
 */
int Gridmap_Iterate(Gridmap* gridmap, Gridmap_IterateCallback callback,
    void* paramaters);


/**
 * Iterate a subset of the blocks of the gridmap and calling func for each.
 *
 * @param xl  Min X
 * @param xh  Max X
 * @param yl  Min Y
 * @param yh  Max Y
 * @param callback  Callback to be made for each block.
 * @param param  Miscellaneous data to be passed in the callback.
 *
 * @return  @c true, iff all callbacks return @c true;
 */
int Gridmap_BoxIterate(Gridmap* gridmap, uint xl, uint xh, uint yl, uint yh,
    Gridmap_IterateCallback callback, void* paramaters);

int Gridmap_BoxIteratev(Gridmap* gridmap, const uint box[4],
    Gridmap_IterateCallback callback, void* paramates);

#endif /* LIBDENG_DATA_GRIDMAP_H */
