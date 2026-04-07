/** @file intermission.h  Hexen specific intermission screens.
 *
 * @authors Copyright © 2009-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBHEXEN_INTERMISSION_H
#define LIBHEXEN_INTERMISSION_H

#include "jhexen.h"
#include <doomsday/uri.h>

struct wbstartstruct_t;

#ifdef __cplusplus

struct wbstartstruct_t
{
    res::Uri nextMap;
    uint nextMapEntryPoint;
};

/**
 * Begin the intermission using the given game session and player configuration.
 *
 * @param wbstartstruct  Configuration to use for the intermission. Ownership is
 *                       @em not given to IN_Begin() however it is assumed that
 *                       this structure is @em not modified while the intermission
 *                       is in progress.
 */
void IN_Begin(const wbstartstruct_t &wbstartstruct);

extern "C" {
#endif

/**
 * End the current intermission.
 */
void IN_End();

/**
 * Process game tic for the intermission.
 *
 * @note Handles user input due to timing issues in netgames.
 */
void IN_Ticker();

/**
 * Draw the intermission.
 */
void IN_Drawer();

/**
 * Change the current intermission state.
 */
void IN_SetState(int stateNum /*interludestate_t st*/);

//void IN_SetTime(int time);

/**
 * Skip to the next state in the intermission.
 */
void IN_SkipToNext();

/// To be called to register the console commands and variables of this module.
void IN_ConsoleRegister();

#ifdef __cplusplus
} // extern "C
#endif

#endif  // LIBHEXEN_INTERMISSION_H
