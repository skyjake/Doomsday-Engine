/**
 * @file audiodriver_music.h
 * Low-level music interface of the audio driver. @ingroup audio
 *
 * The main purpose of this low-level thin layer is to group individual music
 * interfaces together as an aggregate that can be treated as one interface.
 *
 * @todo  Integrate the CD interface into this, too, as it is treated as an
 *        additional/alternative music interface.
 *
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_AUDIODRIVER_MUSIC_H
#define LIBDENG_AUDIODRIVER_MUSIC_H

#include "dd_string.h"
#include "audiodriver.h"

#ifdef __cplusplus
extern "C" {
#endif

void AudioDriver_Music_SetSoundFont(const char* fileName);

int AudioDriver_Music_PlayNativeFile(const char* fileName, boolean looped);

int AudioDriver_Music_PlayLump(lumpnum_t lump, boolean looped);

int AudioDriver_Music_PlayFile(const char* virtualOrNativePath, boolean looped);

boolean AudioDriver_Music_IsPlaying(void);

/**
 * Tells the audio driver to choose a new name for the buffer filename, if a
 * buffer file is needed to play back a song.
 */
void AudioDriver_Music_SwitchBufferFilenames(void);

AutoStr* AudioDriver_Music_ComposeTempBufferFilename(const char* ext);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDENG_AUDIODRIVER_MUSIC_H
