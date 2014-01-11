/** @file p_xg.h Extended Generalised Line / Sector Types.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBCOMMON_PLAYSIM_XG_H
#define LIBCOMMON_PLAYSIM_XG_H

#include "p_xgline.h"
#include "p_xgsec.h"

DENG_EXTERN_C int xgDev;
DENG_EXTERN_C dd_bool xgDataLumps;

#ifdef __cplusplus
extern "C" {
#endif

// Debug message printer.
void XG_Dev(char const *format, ...) PRINTF_F(1,2);

// Called once post init.
void XG_ReadTypes(void);

// Init both XG lines and sectors. Called for each map.
void XG_Init(void);

// Thinks for XG lines and sectors.
void XG_Ticker(void);

// Updates XG state during engine reset.
void XG_Update(void);

void XG_ReadTypes(void);

linetype_t *XG_GetLumpLine(int id);
sectortype_t *XG_GetLumpSector(int id);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBCOMMON_PLAYSIM_XG_H */
