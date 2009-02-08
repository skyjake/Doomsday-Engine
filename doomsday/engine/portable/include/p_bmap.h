/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * p_bmap.h: Blockmaps
 */

#ifndef __DOOMSDAY_PLAYSIM_BLOCKMAP_H__
#define __DOOMSDAY_PLAYSIM_BLOCKMAP_H__

//// \todo This stuff is obsolete and needs to be removed!
#define MAPBLOCKUNITS   128
#define MAPBLOCKSIZE    (MAPBLOCKUNITS*FRACUNIT)
#define MAPBLOCKSHIFT   (FRACBITS+7)
#define MAPBMASK        (MAPBLOCKSIZE-1)
#define MAPBTOFRAC      (MAPBLOCKSHIFT-FRACBITS)

byte bmapShowDebug;
float bmapDebugSize;

// Alloc/dealloc:
blockmap_t*     P_BlockmapCreate(const pvec2_t min, const pvec2_t max,
                                 uint width, uint height);

// Management:
void            P_BlockmapSetBlock(blockmap_t* bmap, uint x, uint y,
                                   linedef_t** lines, linkmobj_t* moLink,
                                   linkpolyobj_t* poLink);
void            P_SSecBlockmapSetBlock(blockmap_t* bmap, uint x, uint y,
                                       subsector_t** ssecs);
void            P_BuildSubsectorBlockMap(gamemap_t* map);

void            P_BlockmapLinkMobj(blockmap_t* bmap, mobj_t* mo);
boolean         P_BlockmapUnlinkMobj(blockmap_t* bmap, mobj_t* mo);
void            P_BlockmapLinkPolyobj(blockmap_t* bmap, polyobj_t* po);
void            P_BlockmapUnlinkPolyobj(blockmap_t* bmap, polyobj_t* po);

// Utility:
void            P_GetBlockmapBounds(blockmap_t* bmap, pvec2_t min, pvec2_t max);
void            P_GetBlockmapDimensions(blockmap_t* bmap, uint v[2]);
boolean         P_ToBlockmapBlockIdx(blockmap_t* bmap, uint destBlock[2],
                                     const pvec2_t sourcePos);
void            P_BoxToBlockmapBlocks(blockmap_t* bmap, uint blockBox[4],
                                      const arvec2_t box);

// Block Iterators:
boolean         P_BlockmapMobjsIterator(blockmap_t* bmap, const uint block[2],
                                         boolean (*func) (struct mobj_s*, void*),
                                         void* data);
boolean         P_BlockmapLinesIterator(blockmap_t* bmap, const uint block[2],
                                        boolean (*func) (linedef_t*, void*),
                                        void* data);
boolean         P_BlockmapSubsectorsIterator(blockmap_t* bmap, const uint block[2],
                                             sector_t* sector, const arvec2_t box,
                                             int localValidCount,
                                             boolean (*func) (subsector_t*, void*),
                                             void* data);
boolean         P_BlockmapPolyobjsIterator(blockmap_t* bmap, const uint block[2],
                                           boolean (*func) (polyobj_t*, void*),
                                           void* data);
boolean         P_BlockmapPolyobjLinesIterator(blockmap_t* bmap, const uint block[2],
                                               boolean (*func) (linedef_t*, void*),
                                               void* data);

// Block Box Iterators:
boolean         P_BlockBoxMobjsIterator(blockmap_t* bmap, const uint blockBox[4],
                                         boolean (*func) (struct mobj_s*, void*),
                                         void* data);
boolean         P_BlockBoxLinesIterator(blockmap_t* bmap, const uint blockBox[4],
                                        boolean (*func) (linedef_t*, void*),
                                        void* data);
boolean         P_BlockBoxSubsectorsIterator(blockmap_t* bmap, const uint blockBox[4],
                                             sector_t* sector, const arvec2_t box,
                                             int localValidCount,
                                             boolean (*func) (subsector_t*, void*),
                                             void* data);
boolean         P_BlockBoxPolyobjsIterator(blockmap_t* bmap, const uint blockBox[4],
                                           boolean (*func) (polyobj_t*, void*),
                                           void* data);
boolean         P_BlockBoxPolyobjLinesIterator(blockmap_t* bmap, const uint blockBox[4],
                                               boolean (*func) (linedef_t*, void*),
                                               void* data);

// Specialized Traversals:
boolean         P_BlockPathTraverse(blockmap_t* bmap, const uint start[2],
                                    const uint end[2], const float origin[2],
                                    const float dest[2], int flags,
                                    boolean (*func) (intercept_t*));

// Misc:
void            P_BlockmapDebug(void);
#endif
