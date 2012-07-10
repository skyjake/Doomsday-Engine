/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2009 Daniel Swanson <danij@dengine.net>
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
 * dswinmm.h: Windows Multimedia, Win32 audio driver.
 */

#ifndef __DSWINMM_H__
#define __DSWINMM_H__

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <mmsystem.h>

#include "doomsday.h"
#include "sys_audiod_mus.h"

// Mixer return values.
enum {
    MIX_ERROR = -1,
    MIX_OK
};

// Mixer devices.
enum {
    MIX_CDAUDIO,
    MIX_MIDI
};

// Mixer actions.
enum {
    MIX_GET,
    MIX_SET
};

// Mixer controls.
enum {
    MIX_VOLUME // 0-255
};

int             mixer4i(int device, int action, int control, int parm);

// Public music interface.
int             DM_Music_Init(void);
void            DM_Music_Shutdown(void);
void            DM_Music_Reset(void);
void            DM_Music_Update(void);
void            DM_Music_Set(int prop, float value);
int             DM_Music_Get(int prop, void* ptr);
void            DM_Music_Pause(int pause);
void            DM_Music_Stop(void);
void*           DM_Music_SongBuffer(unsigned int length);
int             DM_Music_Play(int looped);

// CD Audio interface:
int             DM_CDAudio_Init(void);
void            DM_CDAudio_Shutdown(void);
void            DM_CDAudio_Update(void);
void            DM_CDAudio_Set(int prop, float value);
int             DM_CDAudio_Get(int prop, void* ptr);
void            DM_CDAudio_Pause(int pause);
void            DM_CDAudio_Stop(void);
int             DM_CDAudio_Play(int track, int looped);

#endif
