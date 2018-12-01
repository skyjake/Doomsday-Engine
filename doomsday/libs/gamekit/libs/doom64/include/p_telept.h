/** @file p_telept.h  Intra map teleporters.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
 * @authors Copyright © 1993-1996 id Software, Inc.
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

#ifndef LIBDOOM64_PLAY_TELEPT_H
#define LIBDOOM64_PLAY_TELEPT_H

#ifndef __JDOOM64__
#  error "Using jDoom64 headers without __JDOOM64__"
#endif

#include "doomsday.h"
#include "p_mobj.h"

// Define values for map objects
#define MO_TELEPORTMAN  (14)
#define TELEFOGHEIGHT   (0)

#ifdef __cplusplus
extern "C" {
#endif

int EV_Teleport(Line *line, int side, mobj_t *thing, dd_bool spawnFog);

int EV_FadeSpawn(Line *line, mobj_t *thing);

int EV_FadeAway(Line *line, mobj_t *thing);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDOOM64_PLAY_TELEPT_H
