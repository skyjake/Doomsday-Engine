/**\file
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
 * m_gridmap.h: Generalized blockmap
 */

#ifndef __DOOMSDAY_MISC_GRIDMAP_H__
#define __DOOMSDAY_MISC_GRIDMAP_H__

typedef void* gridmap_t;

gridmap_t  *M_GridmapCreate(uint width, uint height, size_t sizeOfBlock,
                            int memzoneTag);
void        M_GridmapDestroy(gridmap_t *gridmap);

void       *M_GridmapGetBlock(gridmap_t *gridmap, uint x, uint y,
                              boolean alloc);

// Iteration
boolean     M_GridmapIterator(gridmap_t *gridmap,
                              boolean (*callback) (void* p, void *ctx),
                              void *param);
boolean     M_GridmapBoxIterator(gridmap_t *gridmap,
                                 uint xl, uint xh, uint yl, uint yh,
                                 boolean (*callback) (void* p, void *ctx),
                                 void *param);
boolean     M_GridmapBoxIteratorv(gridmap_t *gridmap, const uint box[4],
                                  boolean (*callback) (void* p, void *ctx),
                                  void *param);

#endif
