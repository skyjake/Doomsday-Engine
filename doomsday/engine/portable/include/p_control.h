/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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

/*
 * p_control.h: Player Controls
 */

#ifndef __DOOMSDAY_PLAYER_CONTROL_H__
#define __DOOMSDAY_PLAYER_CONTROL_H__

typedef enum togglestate_e {
    TG_OFF 		= 0,
    TG_ON		= 1,
    TG_NEGATIVE	= 2,
	
	// Some aliases:
    TG_POSITIVE	= TG_ON,
    TG_MIDDLE 	= TG_OFF,

	// Special state:
    TG_TOGGLE	= 3
} togglestate_t;

// Impulse numbers are 8-bit unsigned integers.
typedef unsigned char impulse_t;

typedef struct controltoggle_s {
    togglestate_t state;
    uint    time;               // Time of last change.
} controltoggle_t;

typedef struct controlaxis_s {
    controltoggle_t *toggle;    // The toggle that affects this axis control.
    float   pos;                // Possibly affected by toggles.
    float   axisPos;            // Position of the input device axis.
} controlaxis_t;

void        P_RegisterControl(void);
void        P_ControlTableInit(int player);
void        P_ControlShutdown(void);

void        P_RegisterPlayerControl(uint class, const char *name);

int         P_ControlGetToggles(int player);
void        P_ControlReset(int player);

boolean     P_IsValidControl(const char *cmd);
boolean     P_ControlExecute(const char *command);
void        P_ControlTicker(timespan_t time);

uint 	    P_ControlFindAxis(const char *name);
float 	    P_ControlGetAxis(int player, const char *name);
const char *P_ControlGetAxisName(uint index);
void        P_ControlSetAxis(int player, uint axisControlIndex, float pos);
void        P_ControlAxisDelta(int player, uint axisControlIndex,
                               float delta);
#endif
