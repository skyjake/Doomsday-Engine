/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
 * d_main.h:
 */

#ifndef __D_MAIN_H__
#define __D_MAIN_H__

#ifndef __JDOOM64__
#  error "Using jDoom64 headers without __JDOOM64__"
#endif

#include <stdio.h>

#include "doomdef.h"

extern int verbose;
extern boolean devParm;
extern boolean noMonstersParm;
extern boolean respawnParm;
extern boolean fastParm;
extern boolean turboParm;
extern float turboMul;
extern skillmode_t startSkill;
extern uint startEpisode;
extern uint startMap;
extern boolean autoStart;
extern FILE *debugFile;
extern gamemode_t gameMode;
extern int gameModeBits;
extern char gameModeString[];
extern boolean monsterInfight;
extern char title[];
extern int demoSequence;
extern int pageTic;
extern char *pageName;
extern char *borderLumps[];

void            G_PostInit(void);
void            G_PreInit(void);
void            G_DetectIWADs(void);
void            G_IdentifyVersion(void);
void            G_Shutdown(void);
void            G_EndFrame(void);
void            G_Ticker(timespan_t ticLength);

boolean         G_SetGameMode(gamemode_t mode);

#endif
