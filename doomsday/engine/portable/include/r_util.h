/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * r_util.h: Refresh Utility Routines
 */

#ifndef __DOOMSDAY_REFRESH_UTIL_H__
#define __DOOMSDAY_REFRESH_UTIL_H__

int             R_PointOnSide(const float x, const float y,
                              const partition_t* par);
angle_t         R_PointToAngle(float x, float y);
angle_t         R_PointToAngle2(const float x1, const float y1,
                                const float x2, const float y2);
float           R_PointToDist(const float x, const float y);
linedef_t*      R_GetLineForSide(const uint sideIDX);
subsector_t*    R_PointInSubsector(const float x, const float y);
boolean         R_IsPointInSector(const float x, const float y,
                                  const sector_t* sector);
boolean         R_IsPointInSector2(const float x, const float y,
                                   const sector_t* sector);
boolean         R_IsPointInSubsector(const float x, const float y,
                                     const subsector_t* ssec);
void            R_ScaleAmbientRGB(float* out, const float* in, float mul);
void            R_HSVToRGB(float* rgb, float h, float s, float v);
sector_t*       R_GetSectorForOrigin(const void* ddMobjBase);

#endif
