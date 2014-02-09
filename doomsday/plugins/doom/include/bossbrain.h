/** @file bossbrain.h  Playsim "Boss Brain" management.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1999 Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman (PrBoom 2.2.6)
 * @authors Copyright © 1999-2000 Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze (PrBoom 2.2.6)
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

#ifndef LIBDOOM_PLAY_BOSSBRAIN_H
#define LIBDOOM_PLAY_BOSSBRAIN_H

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

#include "doomsday.h"
#include "p_mobj.h"

/**
 * Global state of boss brain.
 */
typedef struct bossbrain_s {
    int easy;
    int targetOn;
    int numTargets;
    int maxTargets;
    mobj_t **targets;
} bossbrain_t;

DENG_EXTERN_C bossbrain_t bossBrain;

#ifdef __cplusplus
extern "C" {
#endif

void P_BrainInitForMap(void);
void P_BrainShutdown(void);
void P_BrainClearTargets(void);
void P_BrainWrite(Writer *writer);
void P_BrainRead(Reader *reader, int mapVersion);
void P_BrainAddTarget(mobj_t *mo);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDOOM_PLAY_BOSSBRAIN_H
