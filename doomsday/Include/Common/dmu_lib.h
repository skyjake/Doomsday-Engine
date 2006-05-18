/* DE1: $Id$
 * Copyright (C) 2003, 2004 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * DMU_lib.h: Helper routines for accessing the DMU API
 */

#ifndef __DMU_LIB_H__
#define __DMU_LIB_H__

#include "../doomsday.h"

line_t     *P_AllocDummyLine(void);
void        P_FreeDummyLine(line_t* line);

void        P_CopyLine(line_t* from, line_t* to);
void        P_CopySector(sector_t* from, sector_t* to);

int         P_SectorLight(sector_t* sector);
void        P_SectorSetLight(sector_t* sector, int level);
void        P_SectorModifyLight(sector_t* sector, int value);
fixed_t     P_SectorLightx(sector_t* sector);
void        P_SectorModifyLightx(sector_t* sector, fixed_t value);
void       *P_SectorSoundOrigin(sector_t *sec);

#endif
