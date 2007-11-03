/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2007 Daniel Swanson <danij@dengine.net>
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

gridmap_t  *M_GridmapCreate(int width, int height, size_t sizeOfBlock,
                            int memzoneTag,
                            int (*setBlock)(void *p, void *ctx));
void        M_GridmapDestroy(gridmap_t *gridmap);

boolean     M_GridmapSetBlock(gridmap_t *gridmap, int x, int y, void *ctx);

// Iteration
boolean     M_GridmapIterator(gridmap_t *gridmap,
                              boolean (*func) (void *p, void *ctx),
                              void *param);
boolean     M_GridmapBoxIterator(gridmap_t *gridmap,
                                 int xl, int xh, int yl, int yh,
                                 boolean (*func) (void *p, void *ctx),
                                 void *param);

#endif
