/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2012 Daniel Swanson <danij@dengine.net>
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

/*
 * Sound Subsystem.
 */

#ifndef LIBDENG_SOUND_MAIN_H
#define LIBDENG_SOUND_MAIN_H

#include "p_object.h"
#include "def_main.h"
#include "sys_audiod.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup soundPlayFlags Sound Start Flags
 * @ingroup flags
 * @{
 */
#define SF_RANDOM_SHIFT     0x1    ///< Random frequency shift.
#define SF_RANDOM_SHIFT2    0x2    ///< 2x bigger random frequency shift.
#define SF_GLOBAL_EXCLUDE   0x4    ///< Exclude all emitters.
#define SF_NO_ATTENUATION   0x8    ///< Very, very loud...
#define SF_REPEAT           0x10   ///< Repeats until stopped.
#define SF_DONT_STOP        0x20   ///< Sound can't be stopped while playing.
/// @}

extern int showSoundInfo;
extern int soundMinDist, soundMaxDist;
extern int sfxVolume, musVolume;

void S_Register(void);
boolean S_Init(void);
void S_Shutdown(void);
void S_MapChange(void);

/**
 * Must be called after the map has been changed.
 */
void S_SetupForChangedMap(void);

void S_Reset(void);
void S_StartFrame(void);
void S_EndFrame(void);

/**
 * Gets information about a defined sound. Linked sounds are resolved.
 *
 * @param soundID  ID number of the sound.
 * @param freq     Defined frequency for the sound is returned here. May be @c NULL.
 * @param volume   Defined volume for the sound is returned here. May be @c NULL.
 *
 * @return  Sound info (from definitions).
 */
sfxinfo_t* S_GetSoundInfo(int soundID, float* freq, float* volume);

mobj_t* S_GetListenerMobj(void);
int S_LocalSoundAtVolumeFrom(int soundId, mobj_t* origin, coord_t* fixedpos, float volume);
int S_LocalSoundAtVolume(int soundId, mobj_t* origin, float volume);
int S_LocalSound(int soundId, mobj_t* origin);
int S_LocalSoundFrom(int soundId, coord_t* fixedpos);
int S_StartSound(int soundId, mobj_t* origin);
int S_StartSoundEx(int soundId, mobj_t* origin);
int S_StartSoundAtVolume(int soundId, mobj_t* origin, float volume);
int S_ConsoleSound(int soundId, mobj_t* origin, int targetConsole);

/**
 * Stop playing sound(s), either by their unique identifier or by their emitter(s).
 *
 * @param soundId       @c 0= stops all sounds emitted from the targeted origin(s).
 * @param origin        @c NULL= stops all sounds with the ID.
 *                      Otherwise both ID and origin must match.
 * @param flags         @ref soundStopFlags
 */
void S_StopSound2(int soundId, mobj_t* origin, int flags);
void S_StopSound(int soundId, mobj_t* origin/*flags=0*/);

int S_IsPlaying(int soundId, mobj_t* emitter);
int S_StartMusic(const char* musicid, boolean looped);
void S_StopMusic(void);
void S_PauseMusic(boolean paused);
void S_Drawer(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /// LIBDENG_SOUND_MAIN_H
