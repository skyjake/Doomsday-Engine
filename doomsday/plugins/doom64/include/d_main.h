/**\file d_main.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBJDOOM64_MAIN_H
#define LIBJDOOM64_MAIN_H

#ifndef __JDOOM64__
#  error "Using jDoom64 headers without __JDOOM64__"
#endif

#include "doomdef.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int verbose;

extern boolean noMonstersParm; // checkparm of -nomonsters
extern boolean respawnParm; // checkparm of -respawn
extern boolean turboParm; // checkparm of -turbo
//extern boolean randomClassParm; // checkparm of -randclass
extern boolean devParm; // checkparm of -devparm
extern boolean fastParm; // checkparm of -fast

extern float turboMul; // Multiplier for turbo.

extern gamemode_t gameMode;
extern int gameModeBits;

extern char* borderGraphics[];

extern const float defFontRGB[];
extern const float defFontRGB2[];
extern const float defFontRGB3[];

extern boolean monsterInfight;

void D_PreInit(void);
void D_PostInit(void);
void D_Shutdown(void);

void D_EndFrame(void);

int D_GetInteger(int id);
void* D_GetVariable(int id);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBJDOOM64_MAIN_H */
