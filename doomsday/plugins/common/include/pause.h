/**
 * @file pause.h
 * Pausing the game. @ingroup libcommon
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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
DENG_EXTERN_C int paused;

DENG_EXTERN_C int gamePauseWhenFocusLost;
DENG_EXTERN_C int gameUnpauseWhenFocusGained;
DENG_EXTERN_C int gamePauseAfterMapStartTics;

#ifdef __cplusplus
extern "C" {
#endif

D_CMD(Pause);

void Pause_Register(void);

/**
 * Determines if the game is currently paused.
 */
boolean Pause_IsPaused(void);

/**
 * Determines if the game has been paused by the user rather than by force or
 * implicitly when the menu is open. The "PAUSED" indicator should only be
 * drawn when the game is paused by the user.
 */
boolean Pause_IsUserPaused(void);

void Pause_Begin(boolean yes);

void Pause_End(void);

void Pause_BeginForcedPeriod(int tics);

#ifdef __cplusplus
}
#endif

#endif // LIBCOMMON_PAUSE_H
