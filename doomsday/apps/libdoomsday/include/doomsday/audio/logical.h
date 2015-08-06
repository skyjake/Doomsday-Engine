/** @file logical.h  Logical Sound Manager.
 *
 * The Logical Sound Manager. Tracks all currently playing sounds in the world,
 * regardless of whether Sfx is available or if the sounds are actually audible
 * to anyone.
 *
 * Uses PU_MAP, so this has to be inited for every map. (Done via S_MapChange()).
 *
 * @todo This should be part of an audio system base class that can be used
 * both by the client and the server. -jk

 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2015 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDOOMSDAY_AUDIO_LOGICAL_H
#define LIBDOOMSDAY_AUDIO_LOGICAL_H

#include "../libdoomsday.h"
#include "../world/mobj.h"
#include <de/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the Logical Sound Manager for a new map.
 */
LIBDOOMSDAY_PUBLIC void Sfx_InitLogical(void);

/**
 * Remove stopped logical sounds from the hash.
 */
LIBDOOMSDAY_PUBLIC void Sfx_PurgeLogical(void);

/**
 * The sound is entered into the list of playing sounds. Called when a 'world class'
 * sound is started, regardless of whether it's actually started on the local system.
 */
LIBDOOMSDAY_PUBLIC void Sfx_StartLogical(int id, mobj_t *origin, dd_bool isRepeating);

/**
 * The sound is removed from the list of playing sounds. Called whenever a sound is
 * stopped, regardless of whether it was actually playing on the local system.
 *
 * @note If @a id == 0 and  @a origin == 0 then stop everything.
 *
 * @return  Number of sounds stopped.
 */
LIBDOOMSDAY_PUBLIC int Sfx_StopLogical(int id, mobj_t *origin);

/**
 * Returns true if the sound is currently playing somewhere in the world.
 * It doesn't matter if it's audible or not.
 *
 * @param id  @c 0= true if any sounds are playing using the specified @a origin.
 */
LIBDOOMSDAY_PUBLIC dd_bool Sfx_IsPlaying(int id, mobj_t *origin);

LIBDOOMSDAY_PUBLIC void Sfx_Logical_SetOneSoundPerEmitter(dd_bool enabled);

LIBDOOMSDAY_PUBLIC void Sfx_Logical_SetSampleLengthCallback(uint (*callback)(int));

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // LIBDOOMSDAY_AUDIO_LOGICAL_H
