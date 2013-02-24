/** @file s_main.h Sound Subsystem
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBDENG_SOUND_MAIN_H
#define LIBDENG_SOUND_MAIN_H

#include "map/p_object.h"
#include "def_main.h"
#include "api_sound.h"
#include "api_audiod.h"

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
extern int sfxBits, sfxRate;
extern byte sfxOneSoundPerEmitter;

void S_Register(void);

/**
 * Main sound system initialization. Inits both the Sfx and Mus modules.
 *
 * @return  @c true, if there were no errors.
 */
boolean S_Init(void);

/**
 * Shutdown the whole sound system (Sfx + Mus).
 */
void S_Shutdown(void);

/**
 * Must be called after the map has been changed.
 */
void S_SetupForChangedMap(void);

/**
 * Stop all channels and music, delete the entire sample cache.
 */
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
sfxinfo_t *S_GetSoundInfo(int soundID, float *freq, float *volume);

mobj_t *S_GetListenerMobj(void);

void S_Drawer(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /// LIBDENG_SOUND_MAIN_H
