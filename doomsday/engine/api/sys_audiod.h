/**\file sys_audiod.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * Audio Driver.
 */

#ifndef LIBDENG_AUDIO_DRIVER_H
#define LIBDENG_AUDIO_DRIVER_H

typedef enum {
    AUDIOD_DUMMY,
    AUDIOD_SDL_MIXER,
    AUDIOD_OPENAL,
    AUDIOD_FMOD,
    AUDIOD_DSOUND,  // Win32 only
    AUDIOD_WINMM,   // Win32 only
    AUDIODRIVER_COUNT
} audiodriver_e;

#define VALID_AUDIODRIVER_IDENTIFIER(id)    ((id) >= AUDIOD_DUMMY && (id) < AUDIODRIVER_COUNT)

// Audio driver properties.
enum {
    AUDIOP_SOUNDFONT_FILENAME
};

typedef struct audiodriver_s {
    int (*Init) (void);
    void (*Shutdown) (void);
    void (*Event) (int type);
    int (*Set) (int prop, const void* ptr);
} audiodriver_t;

#endif /* LIBDENG_AUDIO_DRIVER_H */
