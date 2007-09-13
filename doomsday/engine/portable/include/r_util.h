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
 * r_util.h: Refresh Utility Routines
 */

#ifndef __DOOMSDAY_REFRESH_UTIL_H__
#define __DOOMSDAY_REFRESH_UTIL_H__

int             R_PointOnSide(fixed_t x, fixed_t y, node_t *node);
angle_t         R_PointToAngle(fixed_t x, fixed_t y);
angle_t         R_PointToAngle2(fixed_t x1, fixed_t y1, fixed_t x2,
                                fixed_t y2);
fixed_t         R_PointToDist(fixed_t x, fixed_t y);
line_t         *R_GetLineForSide(uint sideIDX);
subsector_t    *R_PointInSubsector(fixed_t x, fixed_t y);
boolean         R_IsPointInSector(fixed_t x, fixed_t y, sector_t *sector);
boolean         R_IsPointInSector2(fixed_t x, fixed_t y, sector_t *sector);
void            R_ScaleAmbientRGB(float *out, const float *in, float mul);
sector_t*       R_GetSectorForDegen(void *degenmobj);

#endif
