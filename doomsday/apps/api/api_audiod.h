/**
 * @file api_audiod.h
 * Audio driver interface. @ingroup audio
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DE_AUDIO_DRIVER_INTERFACE_H
#define DE_AUDIO_DRIVER_INTERFACE_H

/**
 * @defgroup audio Audio
 */
///@{

typedef enum audiodriverid_e {
    AUDIOD_INVALID = -1,
    AUDIOD_DUMMY = 0,
    AUDIOD_SDL_MIXER,
    AUDIOD_OPENAL,
    AUDIOD_FMOD,
    AUDIOD_FLUIDSYNTH,
    AUDIOD_DSOUND,  // Win32 only
    AUDIOD_WINMM,   // Win32 only
    AUDIODRIVER_COUNT
} audiodriverid_t;

typedef enum {
    AUDIO_INONE,
    AUDIO_ISFX,
    AUDIO_IMUSIC,
    AUDIO_ICD,
    AUDIO_INTERFACE_COUNT,
    AUDIO_IMUSIC_OR_ICD // forAllInterfaces() special value
} audiointerfacetype_t;

#if defined(DE_WINDOWS)
#  define VALID_AUDIODRIVER_IDENTIFIER(id)    ((id) >= AUDIOD_DUMMY && (id) < AUDIODRIVER_COUNT)
#else
#  define VALID_AUDIODRIVER_IDENTIFIER(id)    ((id) >= AUDIOD_DUMMY && (id) <= AUDIOD_FLUIDSYNTH)
#endif

// Audio driver properties.
enum {
    AUDIOP_SOUNDFONT_FILENAME,
    AUDIOP_SFX_INTERFACE                ///< audiointerface_sfx_t to play sounds with
};

typedef struct audiodriver_s {
    int (*Init) (void);
    void (*Shutdown) (void);
    void (*Event) (int type);
    int (*Set) (int prop, const void *ptr);
} audiodriver_t;

typedef struct audiointerface_base_s {
    int (*Init) (void);
} audiointerface_base_t;

///@}

#endif /* DE_AUDIO_DRIVER_INTERFACE_H */
