/** @file x_main.h  Hexen-specific game initialization
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2009-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBJHEXEN_MAIN_H
#define LIBJHEXEN_MAIN_H

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

#include "h2def.h"

DE_EXTERN_C gamemode_t gameMode;
DE_EXTERN_C int gameModeBits;

DE_EXTERN_C const char *borderGraphics[];

// Default font colors.
DE_EXTERN_C float const defFontRGB[];
DE_EXTERN_C float const defFontRGB2[];
DE_EXTERN_C float const defFontRGB3[];

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Pre Game Initialization routine.
 * All game-specific actions that should take place at this time go here.
 */
void X_PreInit(void);

/**
 * Post Game Initialization routine.
 * All game-specific actions that should take place at this time go here.
 */
void X_PostInit(void);

void X_Shutdown(void);

/**
 * Get a 32-bit integer value.
 */
int X_GetInteger(int id);

/**
 * Get a pointer to the value of a named variable/constant.
 */
void *X_GetVariable(int id);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBJHEXEN_MAIN_H
