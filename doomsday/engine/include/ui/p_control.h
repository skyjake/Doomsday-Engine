/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
 * p_control.h: Player Controls
 */

#ifndef __DOOMSDAY_PLAYER_CONTROL_H__
#define __DOOMSDAY_PLAYER_CONTROL_H__

#include "api_player.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct playercontrol_s {
    int     id;
    controltype_t type;
    boolean isTriggerable; ///< Accepts triggered states in addition to active ones.
    char*   name;
    char*   bindContextName;
} playercontrol_t;

void        P_ControlRegister(void);
void        P_ControlShutdown(void);
void        P_ControlTicker(timespan_t time);

playercontrol_t* P_PlayerControlById(int id);
playercontrol_t* P_PlayerControlByName(const char* name);
void        P_ImpulseByName(int playerNum, const char* control);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
