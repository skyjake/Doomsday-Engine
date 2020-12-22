/** @file dswinmm.h  Windows Multimedia audio driver.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef __DSWINMM_H__
#define __DSWINMM_H__

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <mmsystem.h>

#include "doomsday.h"
#include "api_audiod_mus.h"

DE_USING_API(Con);

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

int mixer4i(int device, int action, int control, int parm);

/// Public music interface -----------------------------------------------------

int DM_Music_Init();
void DM_Music_Shutdown();
void DM_Music_Reset();
void DM_Music_Update();
void DM_Music_Set(int prop, float value);
int DM_Music_Get(int prop, void *ptr);
void DM_Music_Pause(int pause);
void DM_Music_Stop();
void *DM_Music_SongBuffer(unsigned int length);
int DM_Music_Play(int looped);

/// CD Audio interface ---------------------------------------------------------

/**
 * Initialize the CDAudio-interface.
 */
int DM_CDAudio_Init();

/**
 * Shutdown the CDAudio-interface, we do nothing whilst offline.
 */
void DM_CDAudio_Shutdown();

/**
 * Do any necessary update tasks. Called every frame by the engine.
 */
void DM_CDAudio_Update();

/**
 * Assign the value of a CDAudio-interface property.
 */
void DM_CDAudio_Set(int prop, float value);

/**
 * Retrieve the value of a CDAudio-interface property.
 */
int DM_CDAudio_Get(int prop, void *ptr);

/**
 * Pauses playback of the currently playing audio track.
 */
void DM_CDAudio_Pause(int pause);

/**
 * Stops playback of the currently playing audio track.
 */
void DM_CDAudio_Stop();

/**
 * Begin playback of a specifc audio track, possibly looped.
 */
int DM_CDAudio_Play(int track, int looped);

#endif // __DSWINMM_H__
