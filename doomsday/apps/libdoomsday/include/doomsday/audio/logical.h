/** @file logical.h  Logical Sound Manager.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

LIBDOOMSDAY_PUBLIC void Sfx_InitLogical(void);
LIBDOOMSDAY_PUBLIC void Sfx_PurgeLogical(void);
LIBDOOMSDAY_PUBLIC void Sfx_StartLogical(int id, mobj_t *origin, dd_bool isRepeating);
LIBDOOMSDAY_PUBLIC int Sfx_StopLogical(int id, mobj_t *origin);
LIBDOOMSDAY_PUBLIC dd_bool Sfx_IsPlaying(int id, mobj_t *origin);

LIBDOOMSDAY_PUBLIC void Sfx_Logical_SetOneSoundPerEmitter(dd_bool enabled);
LIBDOOMSDAY_PUBLIC void Sfx_Logical_SetSampleLengthCallback(uint (*callback)(int));

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDOOMSDAY_AUDIO_LOGICAL_H
