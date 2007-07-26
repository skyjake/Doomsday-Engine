/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Jamie Jones <yagisan@dengine.net>
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
 * sys_input.h: Game Controllers
 */

#ifndef __DOOMSDAY_INPUT_H__
#define __DOOMSDAY_INPUT_H__

// Key event types.
#define IKE_NONE		0
#define IKE_KEY_DOWN	0x1
#define IKE_KEY_UP		0x2

// Mouse buttons.
#define IMB_LEFT		0x1
#define IMB_RIGHT		0x2
#define IMB_MIDDLE		0x4
#define IMB_FOUR		0x8
#define IMB_FIVE		0x10
#define IMB_SIX			0x20
#define IMB_SEVEN		0x40
#define IMB_EIGHT		0x80
#define IMB_MAXBUTTONS  8

// Joystick.
#define IJOY_AXISMIN	-10000
#define IJOY_AXISMAX	10000
#define IJOY_MAXAXES    32
#define IJOY_MAXBUTTONS	32
#define IJOY_MAXHATS	4
#define IJOY_POV_CENTER	-1

typedef struct {
	char            event;          // Type of the event.       
	unsigned char   code;           // The scancode (extended, corresponds DI keys).
} keyevent_t;

typedef struct {
	int             x, y, z;        // Relative X and Y mickeys since last call.
	int             buttons;        // The buttons bitfield.
} mousestate_t;

typedef struct {
    int             numAxes;        // Number of axes present.
    int             numButtons;     // Number of buttons present.
    int             numHats;        // Number of hats present.
	int             axis[IJOY_MAXAXES];
	char            buttons[IJOY_MAXBUTTONS];
	float           hatAngle[IJOY_MAXHATS];	   // 0 - 359 degrees.
} joystate_t;

extern byte     usejoystick;

void            I_Register(void);
int             I_Init(void);
void            I_Shutdown(void);
boolean         I_MousePresent(void);
boolean         I_JoystickPresent(void);
int             I_GetKeyEvents(keyevent_t *evbuf, int bufsize);
void            I_GetMouseState(mousestate_t *state);
void            I_GetJoystickState(joystate_t * state);

#endif
