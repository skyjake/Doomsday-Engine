/**\file
 *\section License
 * License: GPL + jHeretic/jHexen Exception
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
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
 *
 * In addition, as a special exception, we, the authors of deng
 * give permission to link the code of our release of deng with
 * the libjhexen and/or the libjheretic libraries (or with modified
 * versions of it that use the same license as the libjhexen or
 * libjheretic libraries), and distribute the linked executables.
 * You must obey the GNU General Public License in all respects for
 * all of the code used other than “libjhexen or libjheretic”. If
 * you modify this file, you may extend this exception to your
 * version of the file, but you are not obligated to do so. If you
 * do not wish to do so, delete this exception statement from your version.
 */

/**
 * h_main.h:
 */

#ifndef __H_MAIN_H__
#define __H_MAIN_H__

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
extern boolean artiSkipParm;

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
