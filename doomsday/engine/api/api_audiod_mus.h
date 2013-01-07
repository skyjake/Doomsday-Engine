/**
 * @file api_audiod_mus.h
 * Music interface for an audio driver. @ingroup audio
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef __DOOMSDAY_AUDIO_DRIVER_MUSIC_H__
#define __DOOMSDAY_AUDIO_DRIVER_MUSIC_H__

/// @addtogroup audio
///@{

/// Music interface properties.
enum {
    MUSIP_ID,       ///< Only for Get()ing.
    MUSIP_PLAYING,  ///< Is playback in progress?
    MUSIP_VOLUME
};

/// Generic driver interface. All other interfaces are based on this.
typedef struct audiointerface_music_generic_s {
    int             (*Init) (void);
    void            (*Shutdown) (void);
    void            (*Update) (void);
    void            (*Set) (int prop, float value);
    int             (*Get) (int prop, void *value);
    void            (*Pause) (int pause);
    void            (*Stop) (void);
} audiointerface_music_generic_t;

/// Driver interface for playing music.
typedef struct audiointerface_music_s {
    audiointerface_music_generic_t gen;
    void*           (*SongBuffer) (unsigned int length);
    int             (*Play) (int looped);
    int             (*PlayFile) (const char *filename, int looped);
} audiointerface_music_t;

/// Driver interface for playing CD tracks.
typedef struct audiointerface_cd_s {
    audiointerface_music_generic_t gen;
    int             (*Play) (int track, int looped);
} audiointerface_cd_t;

///@}

#endif
