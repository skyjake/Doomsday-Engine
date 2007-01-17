/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
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
 * p_setup.c: Common map setup routines.
 */

#ifndef __COMMON_PLAYSETUP_H__
#define __COMMON_PLAYSETUP_H__

#define numvertexes (*(uint*) DD_GetVariable(DD_VERTEX_COUNT))
#define numsegs     (*(uint*) DD_GetVariable(DD_SEG_COUNT))
#define numsectors  (*(uint*) DD_GetVariable(DD_SECTOR_COUNT))
#define numsubsectors (*(uint*) DD_GetVariable(DD_SUBSECTOR_COUNT))
#define numnodes    (*(uint*) DD_GetVariable(DD_NODE_COUNT))
#define numlines    (*(uint*) DD_GetVariable(DD_LINE_COUNT))
#define numsides    (*(uint*) DD_GetVariable(DD_SIDE_COUNT))
#define numthings   (DD_GetInteger(DD_THING_COUNT))

#if __JHEXEN__
#define numpolyobjs (DD_GetInteger(DD_POLYOBJ_COUNT))
#endif

void        P_SetupLevel(int episode, int map, int playermask, skillmode_t skill);


#endif
