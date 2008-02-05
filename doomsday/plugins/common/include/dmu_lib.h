/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
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
 * DMU_lib.h: Helper routines for accessing the DMU API
 */

#ifndef __DMU_LIB_H__
#define __DMU_LIB_H__

#include "doomsday.h"

linedef_t     *P_AllocDummyLine(void);
void        P_FreeDummyLine(linedef_t* line);

void        P_CopyLine(linedef_t* from, linedef_t* to);
void        P_CopySector(sector_t* from, sector_t* to);

float       P_SectorLight(sector_t* sector);
void        P_SectorSetLight(sector_t* sector, float level);
void        P_SectorModifyLight(sector_t* sector, float value);
void        P_SectorModifyLightx(sector_t* sector, fixed_t value);
void       *P_SectorSoundOrigin(sector_t *sec);

#endif
