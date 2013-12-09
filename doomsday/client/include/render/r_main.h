/** @file r_main.h
 *
 * @author Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_RENDER_R_MAIN_H
#define LIBDENG_RENDER_R_MAIN_H

#include "dd_share.h"

DENG_EXTERN_C float    frameTimePos;      // 0...1: fractional part for sharp game tics
DENG_EXTERN_C int      validCount;

DENG_EXTERN_C int      levelFullBright;

DENG_EXTERN_C float    pspOffset[2], pspLightLevelMultiplier;
DENG_EXTERN_C int      psp3d;
DENG_EXTERN_C float    weaponOffsetScale, weaponFOVShift;
DENG_EXTERN_C int      weaponOffsetScaleY;
DENG_EXTERN_C byte     weaponScaleMode; // cvar

DENG_EXTERN_C byte     precacheMapMaterials, precacheSprites, precacheSkins;

DENG_EXTERN_C fixed_t  fineTangent[FINEANGLES / 2];

DENG_EXTERN_C byte     texGammaLut[256];

void R_BuildTexGammaLut(void);

/**
 * One-time initialization of the refresh daemon. Called by DD_Main.
 */
void R_Init(void);

/**
 * Re-initialize almost everything.
 */
void R_Update(void);

/**
 * Shutdown the refresh daemon.
 */
void R_Shutdown(void);

#endif /* LIBDENG_REFRESH_MAIN_H */
