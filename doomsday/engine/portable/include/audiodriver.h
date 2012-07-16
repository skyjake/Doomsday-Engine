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

#include "dd_string.h"
#include "sys_audiod.h"
#include "sys_audiod_sfx.h"
#include "sys_audiod_mus.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_AUDIO_INTERFACES  16 // arbitrary

boolean AudioDriver_Init(void);
void AudioDriver_Shutdown(void);

/**
 * Prints a list of the selected, active interfaces to the log.
 */
void AudioDriver_PrintInterfaces(void);

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
audiodriver_t* AudioDriver_Interface(void* anyAudioInterface);

AutoStr* AudioDriver_InterfaceName(void* anyAudioInterface);

audiointerfacetype_t AudioDriver_InterfaceType(void* anyAudioInterface);

/**
 * Lists all active interfaces of a given type, in descending priority order:
 * the most important interface is listed first in the returned array.
 * Alternatively, counts the number of active interfaces of a given type.
 *
 * @param type              Type of interface to look for.
 * @param listOfInterfaces  Matching interfaces are written here. Points to an
 *                          array of pointers. If this is @c NULL,
 *                          just counts the number of matching interfaces.
 *
 * @return Number of matching interfaces.
 */
int AudioDriver_FindInterfaces(audiointerfacetype_t type, void** listOfInterfaces);

/**
 * Returns the current active primary SFX interface. @c NULL is returned is no
 * SFX playback is available.
 */
audiointerface_sfx_generic_t* AudioDriver_SFX(void);

/**
 * Determines if at least one music interface is available for music playback.
 */
boolean AudioDriver_Music_Available(void);

/**
 * Returns the currently active CD playback interface. @c NULL is returned if
 * CD playback is not available.
 *
 * @note  The CD interface is considered to belong in the music aggregate
 *        interface (see audiodriver_music.h), and usually does not need to
 *        be individually manipulated.
 */
audiointerface_cd_t* AudioDriver_CD(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDENG_AUDIO_DRIVER_H
