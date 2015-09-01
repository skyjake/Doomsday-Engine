/** @file dswinmm.h  Doomsday audio driver plugin for Windows Multimedia.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef WINMM_MAIN_H
#define WINMM_MAIN_H

#include "doomsday.h"
#include "api_audiod.h"
#include "api_audiod_mus.h"

DENG_USING_API(Con);

// CD-Audio interface: ------------------------------------------------------------------

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

// Music interface: ---------------------------------------------------------------------

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

#endif  // WINMM_MAIN_H
