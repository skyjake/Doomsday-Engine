/** @file p_xg.h  Extended generalised line / sector types.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

/// Indicates XG is available.
#define LIBCOMMON_HAVE_XG 1

DE_EXTERN_C int xgDev;
DE_EXTERN_C dd_bool xgDataLumps;

#ifdef __cplusplus

/*
 * XG debug message logging macros:
 */

#define LOG_MAP_MSG_XGDEVONLY(msg)         if(xgDev) { LOG_MAP_MSG(msg); }
#define LOG_MAP_MSG_XGDEVONLY2(form, args) if(xgDev) { LOG_MAP_MSG(form) << args; }

extern "C" {
#endif

// Called once post init.
void XG_ReadTypes(void);

// Init both XG lines and sectors. Called for each map.
void XG_Init(void);

// Thinks for XG lines and sectors.
void XG_Ticker(void);

// Updates XG state during engine reset.
void XG_Update(void);

/**
 * See if any line or sector types are saved in a DDXGDATA lump.
 */
void XG_ReadTypes(void);

linetype_t *XG_GetLumpLine(int id);
sectortype_t *XG_GetLumpSector(int id);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBCOMMON_PLAYSIM_XG_H
