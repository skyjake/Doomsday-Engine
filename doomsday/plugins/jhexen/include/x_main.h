/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2009 Daniel Swanson <danij@dengine.net>
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
 * x_main.h:
 */

#ifndef __X_MAIN_H__
#define __X_MAIN_H__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

extern boolean noMonstersParm; // checkparm of -nomonsters
extern boolean respawnParm; // checkparm of -respawn
extern boolean turboParm; // checkparm of -turbo
extern boolean randomClassParm; // checkparm of -randclass
extern boolean devParm; // checkparm of -devparm
extern boolean artiSkipParm; // Whether shift-enter skips an artifact.
extern float turboMul; // Multiplier for turbo.

extern int gameModeBits;
extern char gameModeString[];

extern char *borderLumps[];

// Default font colours.
extern const float defFontRGB[];
extern const float defFontRGB2[];

extern boolean useScriptsDir;
extern filename_t scriptsDir;

void            G_DetectIWADs(void);
void            G_PreInit(void);
void            G_PostInit(void);
void            G_Shutdown(void);

void            G_EndFrame(void);

#endif
