/* DE1: $Id$
 * Copyright (C) 2005 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * <file name>: <short summary>
 *
 * <description>
 */

#ifndef __DOOMSDAY_REFRESH_LIGHT_GRID_H__
#define __DOOMSDAY_REFRESH_LIGHT_GRID_H__

void            LG_Register(void);
void            LG_Init(void);
void            LG_SectorChanged(sector_t *sector, sectorinfo_t *info);
void            LG_Update(void);
void            LG_Evaluate(const float *point, byte *color);
void            LG_Debug(void);

#endif
