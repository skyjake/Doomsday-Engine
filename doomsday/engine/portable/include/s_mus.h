/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2009-2011 Daniel Swanson <danij@dengine.net>
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
 * s_mus.h: Music Subsystem
 */

#ifndef __DOOMSDAY_SOUND_MUSIC_H__
#define __DOOMSDAY_SOUND_MUSIC_H__

#include "con_decl.h"
#include "sys_audiod_mus.h"

// Music preference. If multiple resources are available, this setting
// is used to determine which one to use (mus < ext < cd).
enum {
    MUSP_MUS,
    MUSP_EXT,
    MUSP_CD
};

extern int      musPreference;

void            Mus_Register(void);
boolean         Mus_Init(void);
void            Mus_Shutdown(void);
void            Mus_SetVolume(float vol);
void            Mus_Pause(boolean doPause);
void            Mus_StartFrame(void);
int             Mus_Start(ded_music_t* def, boolean looped);
int             Mus_StartLump(lumpnum_t lump, boolean looped, boolean canPlayMUS);
void            Mus_Stop(void);

#endif
