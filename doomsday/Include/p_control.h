/* DE1: $Id$
 * Copyright (C) 2004 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
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
	uint time; 					// Time of last change.
} controltoggle_t;

typedef struct controlaxis_s {
	controltoggle_t *toggle;	// The toggle that affects this axis control.
	float pos;					// Possibly affected by toggles.
	float axisPos;				// Position of the input device axis.
} controlaxis_t;

void 	P_ControlInit(void);
void 	P_ControlShutdown(void);
void 	P_ControlTicker(timespan_t time);
void 	P_ControlTableInit(int player);
void 	P_ControlTableFree(int player);
void	P_ControlReset(void);
int 	P_ControlFindAxis(const char *name);
const char *P_ControlGetAxisName(int index);
boolean	P_ControlExecute(const char *command);
int 	P_ControlGetToggles(int player);
float 	P_ControlGetAxis(int player, const char *name);
void 	P_ControlSetAxis(int player, int axisControlIndex, float pos);
void 	P_ControlAxisDelta(int player, int axisControlIndex, float delta);

#endif 
