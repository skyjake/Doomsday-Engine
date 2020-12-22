/** @file joystick.h  Joystick input pre-processing.
 *
 * @see sys_input.h
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#ifndef DE_CLIENT_UI_JOYSTICK_H
#define DE_CLIENT_UI_JOYSTICK_H

#ifdef __SERVER__
#  error Joystick is not available in a SERVER build
#endif

#include <de/string.h>

#define IJOY_AXISMIN    -10000
#define IJOY_AXISMAX    10000
#define IJOY_MAXAXES    32
#define IJOY_MAXBUTTONS 32
#define IJOY_MAXHATS    4
#define IJOY_POV_CENTER -1

typedef struct joystate_s {
    int numAxes;                       ///< Number of axes present.
    int numButtons;                    ///< Number of buttons present.
    int numHats;                       ///< Number of hats present.
    int axis[IJOY_MAXAXES];
    int buttonDowns[IJOY_MAXBUTTONS];  ///< Button down count.
    int buttonUps[IJOY_MAXBUTTONS];    ///< Button up count.
    float hatAngle[IJOY_MAXHATS];      ///< 0 - 359 degrees.
} joystate_t;

void Joystick_Register();

bool Joystick_Init();

void Joystick_Shutdown();

bool Joystick_IsPresent();

void Joystick_GetState(joystate_t *state);

de::String Joystick_Name();

#endif // DE_CLIENT_UI_JOYSTICK_H
