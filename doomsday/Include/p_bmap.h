/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * p_bmap.h: Blockmaps
 */

#ifndef __DOOMSDAY_PLAYSIM_BLOCKMAP_H__
#define __DOOMSDAY_PLAYSIM_BLOCKMAP_H__

void P_InitSubsectorBlockMap (void);

void P_InitPolyBlockMap (void);

boolean P_BlockLinesIterator (int x, int y,
							  boolean(*func)(line_t*,void*), void *data);

boolean P_BlockPolyobjsIterator (int x, int y,
								 boolean(*func)(polyobj_t*,void*),
								 void *data);

boolean P_SubsectorBoxIteratorv (arvec2_t box, sector_t *sector,
								 boolean (*func)(subsector_t*, void*),
								 void *parm);

boolean P_SubsectorBoxIterator (fixed_t *box, sector_t *sector,
								boolean (*func)(subsector_t*, void*),
								void *parm);

#endif 
