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
 *
 * Based on Hexen by Raven Software.
 */

/*
 * r_util.h: Refresh Utility Routines
 */

#ifndef __DOOMSDAY_REFRESH_UTIL_H__
#define __DOOMSDAY_REFRESH_UTIL_H__

int			R_PointOnSide (fixed_t x, fixed_t y, node_t *node);
angle_t		R_PointToAngle (fixed_t x, fixed_t y);
angle_t		R_PointToAngle2 (fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2);
fixed_t		R_PointToDist (fixed_t x, fixed_t y);
line_t*		R_GetLineForSide(int sideNumber);
subsector_t* R_PointInSubsector (fixed_t x, fixed_t y);
boolean		R_IsPointInSector(fixed_t x, fixed_t y, sector_t *sector);
int			R_GetSectorNumForDegen(void *degenmobj);

#endif 
