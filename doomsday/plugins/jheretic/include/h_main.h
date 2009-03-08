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
 * h_main.h:
 */

#ifndef __JHERETIC_MAIN_H__
#define __JHERETIC_MAIN_H__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#include "doomdef.h"

extern int verbose;

extern boolean devParm;
extern boolean noMonstersParm;
extern boolean respawnParm;
extern boolean turboParm;
extern boolean fastParm;
extern boolean invSkipParam;

extern float turboMul;
extern skillmode_t startSkill;
extern int startEpisode;
extern int startMap;
extern boolean autoStart;
extern gamemode_t gameMode;
extern int gameModeBits;
extern char gameModeString[];
extern boolean monsterInfight;
extern const float defFontRGB[];
extern const float defFontRGB2[];
extern char *borderLumps[];
extern char *wadFiles[];
extern char *baseDefault;
extern char exrnWADs[];
extern char exrnWADs2[];

void            G_Shutdown(void);
void            G_EndFrame(void);
boolean         G_SetGameMode(gamemode_t mode);
void            G_DetectIWADs(void);

#endif
