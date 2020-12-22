/** @file pause.h  Pausing the game.
 * @ingroup libcommon
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_PAUSE_H
#define LIBCOMMON_PAUSE_H

#include "dd_share.h"

/// Non-zero when the game is paused: game time is not advancing.
DE_EXTERN_C int paused;

#ifdef __cplusplus
extern "C" {
#endif

D_CMD(Pause);

void Pause_Register(void);

/**
 * Update the pause state. Called 35 times per second.
 */
void Pause_Ticker(void);

/**
 * Process events related to pausing.
 *
 * @param ev  Event.
 *
 * @return @c true, if the event was processed.
 */
dd_bool Pause_Responder(event_t *ev);

/**
 * Determines if the game is currently paused.
 */
dd_bool Pause_IsPaused(void);

/**
 * Determines if the game has been paused by the user rather than by force or
 * implicitly when the menu is open. The "PAUSED" indicator should only be
 * drawn when the game is paused by the user.
 */
dd_bool Pause_IsUserPaused(void);

/**
 * Sets the user-requested pause mode.
 *
 * @param yes  @c true, if the user has requested pause and game time
 *             should be paused. @c false, to unpause.
 */
void Pause_Set(dd_bool yes);

/**
 * Starts a forced pause period. Game time will not progress during the pause,
 * but the user interface does not indicate that the game is paused.
 *
 * @param tics  Number of tics to pause.
 */
void Pause_SetForcedPeriod(int tics);

/**
 * Unpauses the game if it is currently paused (even if forced).
 */
void Pause_End(void);

/**
 * If so configured, pauses the game for the map start pause period.
 */
void Pause_MapStarted(void);

#ifdef __cplusplus
}
#endif

#endif // LIBCOMMON_PAUSE_H
