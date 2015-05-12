/** @file s_mus.h  Music Subsystem.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2009-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_SOUND_MUSIC_H
#define LIBDENG_SOUND_MUSIC_H

#include "api_audiod_mus.h"
#include <de/Record>

// Music preference. If multiple resources are available, this setting
// is used to determine which one to use (mus < ext < cd).
enum {
    MUSP_MUS,
    MUSP_EXT,
    MUSP_CD
};

/**
 * Register the console commands and variables of this module.
 */
void Mus_Register();

/**
 * Initialize the Mus module.
 * @return  @c true, if no errors occur.
 */
bool Mus_Init();

void Mus_Shutdown();

/**
 * Set the general music volume. Affects all music played by all interfaces.
 */
void Mus_SetVolume(float vol);

/**
 * Pauses or resumes the music.
 */
void Mus_Pause(bool doPause);

/**
 * Called on each frame by S_StartFrame.
 */
void Mus_StartFrame();

/**
 * Start playing a song. The chosen interface depends on what's available
 * and what kind of resources have been associated with the song.
 * Any previously playing song is stopped.
 *
 * @return  Non-zero if the song is successfully played.
 */
int Mus_Start(de::Record const *def, bool looped);

/**
 * @return 1, if music was started. 0, if attempted to start but failed.
 *        -1, if it was MUS data and @a canPlayMUS says we can't play it.
 */
int Mus_StartLump(lumpnum_t lump, bool looped, bool canPlayMUS);

void Mus_Stop();

#endif // LIBDENG_SOUND_MUSIC_H
