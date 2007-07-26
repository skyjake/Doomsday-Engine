/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 by Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman (PrBoom 2.2.6)
 *\author Copyright © 1993-1996 by id Software, Inc.
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
 * p_mapspec.c:
 *
 * Line Tag handling. Line and Sector groups. Specialized iteration
 * routines, respective utility functions...
 */

#ifndef __COMMON_MAP_SPEC_H__
#define __COMMON_MAP_SPEC_H__

#include "p_iterlist.h"

extern iterlist_t *spechit; // for crossed line specials.
extern iterlist_t *linespecials; // for surfaces that tick eg wall scrollers.

void            P_DestroyLineTagLists(void);
iterlist_t     *P_GetLineIterListForTag(int tag, boolean createNewList);

void            P_DestroySectorTagLists(void);
iterlist_t     *P_GetSectorIterListForTag(int tag, boolean createNewList);

sector_t       *P_GetNextSector(line_t *line, sector_t *sec);

float           P_FindLowestFloorSurrounding(sector_t *sec);
float           P_FindHighestFloorSurrounding(sector_t *sec);
float           P_FindNextHighestFloor(sector_t *sec, float currentheight);
float           P_FindLowestCeilingSurrounding(sector_t *sec);
float           P_FindHighestCeilingSurrounding(sector_t *sec);
float           P_FindMinSurroundingLight(sector_t *sector, float min);

#endif
