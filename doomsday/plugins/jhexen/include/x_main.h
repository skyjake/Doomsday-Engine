/**\file x_main.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2009-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBJHEXEN_MAIN_H
#define LIBJHEXEN_MAIN_H

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

extern int verbose;

extern boolean noMonstersParm; // checkparm of -nomonsters
extern boolean respawnParm; // checkparm of -respawn
extern boolean turboParm; // checkparm of -turbo
extern boolean randomClassParm; // checkparm of -randclass
extern boolean devParm; // checkparm of -devparm
//extern boolean fastParm; // checkparm of -fast

extern float turboMul; // Multiplier for turbo.

extern gamemode_t gameMode;
extern int gameModeBits;

extern char* borderGraphics[];

// Default font colors.
extern const float defFontRGB[];
extern const float defFontRGB2[];
extern const float defFontRGB3[];

void X_PreInit(void);
void X_PostInit(void);
void X_Shutdown(void);
int X_GetInteger(int id);
void* X_GetVariable(int id);

#endif /* LIBJHEXEN_MAIN_H */
