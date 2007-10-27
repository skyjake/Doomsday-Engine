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

/*
 * p_bmap.h: Blockmaps
 */

#ifndef __DOOMSDAY_PLAYSIM_BLOCKMAP_H__
#define __DOOMSDAY_PLAYSIM_BLOCKMAP_H__

void            P_InitSubsectorBlockMap(void);

void            P_InitPolyBlockMap(gamemap_t *map);

boolean         P_BlockLinesIterator(int x, int y,
                                     boolean (*func) (line_t *, void *),
                                     void *data);

boolean         P_BlockPolyobjsIterator(int x, int y,
                                        boolean (*func) (polyobj_t *, void *),
                                        void *data);

boolean         P_SubsectorBoxIteratorv(arvec2_t box, sector_t *sector,
                                        boolean (*func) (subsector_t *,
                                                         void *), void *parm);

boolean         P_SubsectorBoxIterator(fixed_t *box, sector_t *sector,
                                       boolean (*func) (subsector_t *, void *),
                                       void *parm);

#endif
