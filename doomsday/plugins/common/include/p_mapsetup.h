/**\file p_mapsetup.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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
 * Common map setup routines
 */

#ifndef LIBCOMMON_PLAYSETUP_H
#define LIBCOMMON_PLAYSETUP_H

#define numvertexes (*(uint*) DD_GetVariable(DD_VERTEX_COUNT))
#define numhedges   (*(uint*) DD_GetVariable(DD_HEDGE_COUNT))
#define numsectors  (*(uint*) DD_GetVariable(DD_SECTOR_COUNT))
#define numbspleafs (*(uint*) DD_GetVariable(DD_BSPLEAF_COUNT))
#define numbspnodes (*(uint*) DD_GetVariable(DD_BSPNODE_COUNT))
#define numlines    (*(uint*) DD_GetVariable(DD_LINE_COUNT))
#define numsides    (*(uint*) DD_GetVariable(DD_SIDE_COUNT))

#if __JHEXEN__
#define numpolyobjs (*(uint*) DD_GetVariable(DD_POLYOBJ_COUNT))
#endif

// If true we are in the process of setting up a map.
extern boolean mapSetup;

void P_SetupForMapData(int type, uint num);

/**
 * Load the specified map.
 */
void P_SetupMap(uint episode, uint map);

const char* P_GetMapNiceName(void);
patchid_t P_FindMapTitlePatch(uint episode, uint map);
const char* P_GetMapAuthor(boolean supressGameAuthor);

#endif /* LIBCOMMON_PLAYSETUP_H */
