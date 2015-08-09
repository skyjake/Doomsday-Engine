/** @file s_mus.h  Music Subsystem.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#ifndef AUDIO_MUSIC_H
#define AUDIO_MUSIC_H

#ifndef __cplusplus
#  error "s_mus.h requires C++"
#endif

#include <de/Record>
#include "dd_types.h"
#include "api_audiod_mus.h"

// Music preference. If multiple resources are available, this setting is used to determine
// which one to use (mus < ext < cd).
enum
{
    MUSP_MUS,
    MUSP_EXT,
    MUSP_CD
};

/**
 * Register the console commands and variables of this module.
 */
void Mus_ConsoleRegister();

/**
 * Initialize the Mus module and return @c true if no errors occur.
 */
bool Mus_Init();

/**
 * Deinitialize the Mus module.
 */
void Mus_Shutdown();

/**
 * Set the general music volume. Affects all music played by all interfaces.
 */
void Mus_SetVolume(float vol);

/**
 * Called on each frame by S_StartFrame.
 */
void Mus_StartFrame();

/**
 * Determines if music is currently playing (on any of the Music or CD audio interfaces).
 */
bool Mus_IsPlaying();

/**
 * Stop the currently playing music, if any.
 */
void Mus_Stop();

/**
 * Pauses or resumes the music.
 */
void Mus_Pause(bool doPause);

/**
 * Start playing a song. The chosen interface depends on what's available and what kind
 * of resources have been associated with the song. Any previously playing song is stopped.
 *
 * @param definition  Music definition describing which associated music file to play.
 *
 * @return  Non-zero if the song is successfully played.
 */
int Mus_Start(de::Record const &definition, bool looped = false);

#endif  // AUDIO_MUSIC_H
