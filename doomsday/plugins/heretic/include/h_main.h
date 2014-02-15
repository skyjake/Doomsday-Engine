/** @file h_main.h  Heretic-specific game initialization.
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

#ifndef LIBJHERETIC_MAIN_H
#define LIBJHERETIC_MAIN_H

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#include "doomdef.h"

DENG_EXTERN_C int verbose;

DENG_EXTERN_C float turboMul; // Multiplier for turbo.

DENG_EXTERN_C gamemode_t gameMode;
DENG_EXTERN_C int gameModeBits;

DENG_EXTERN_C char *borderGraphics[];

DENG_EXTERN_C float const defFontRGB[];
DENG_EXTERN_C float const defFontRGB2[];
DENG_EXTERN_C float const defFontRGB3[];

DENG_EXTERN_C dd_bool monsterInfight;

#ifdef __cplusplus
extern "C" {
#endif

void H_PreInit(void);

void H_PostInit(void);

void H_Shutdown(void);

int H_GetInteger(int id);

void *H_GetVariable(int id);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBJHERETIC_MAIN_H
