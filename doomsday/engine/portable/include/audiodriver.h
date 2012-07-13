/**
 * @file audiodriver.h
 * Audio driver loading and interface management. @ingroup audio
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

#ifndef LIBDENG_AUDIO_DRIVER_H
#define LIBDENG_AUDIO_DRIVER_H

#include "sys_audiod.h"
#include "sys_audiod_sfx.h"
#include "sys_audiod_mus.h"

#ifdef __cplusplus
extern "C" {
#endif

boolean AudioDriver_Init(void);
void AudioDriver_Shutdown(void);

/**
 * Retrieves the main interface of the audio driver to which @a audioInterface
 * belongs.
 *
 * @param audioInterface  Pointer to a SFX, Music, or CD interface. See
 * AudioDriver_SFX(), AudioDriver_Music() and AudioDriver_CD().
 *
 * @return Audio driver interface, or @c NULL if the none of the loaded drivers
 * match.
 */
audiodriver_t* AudioDriver_Interface(void* audioInterface);

/**
 * Returns the current active SFX interface. @c NULL is returned is no SFX
 * playback is available.
 */
audiointerface_sfx_generic_t* AudioDriver_SFX(void);

/**
 * Returns the currently active Music interface. @c NULL is returned if no music
 * playback is available.
 */
audiointerface_music_t* AudioDriver_Music(void);

/**
 * Returns the currently active CD playback interface. @c NULL is returned if
 * CD playback is not available.
 */
audiointerface_cd_t* AudioDriver_CD(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDENG_AUDIO_DRIVER_H
