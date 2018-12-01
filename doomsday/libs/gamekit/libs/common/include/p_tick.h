/** @file p_tick.h  Common top-level tick stuff.
 *
 * @authors Copyright © 2006-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_P_TICK_H
#define LIBCOMMON_P_TICK_H

#include "dd_types.h"
#include "pause.h"

DE_EXTERN_C int mapTime;
DE_EXTERN_C int actualMapTime;
DE_EXTERN_C int timerGame;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * This is called at all times, no matter gamestate.
 */
void P_RunPlayers(timespan_t ticLength);

/**
 * Called 35 times per second.
 * The heart of play sim.
 */
void P_DoTick(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBCOMMON_P_TICK_H
