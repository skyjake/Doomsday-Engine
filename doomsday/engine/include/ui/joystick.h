/**
 * @file joystick.h
 * Joystick input pre-processing. @ingroup input
 *
 * @see sys_input.h
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_SYSTEM_JOYSTICK_H
#define LIBDENG_SYSTEM_JOYSTICK_H

#include "dd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IJOY_AXISMIN    -10000
#define IJOY_AXISMAX    10000
#define IJOY_MAXAXES    32
#define IJOY_MAXBUTTONS 32
#define IJOY_MAXHATS    4
#define IJOY_POV_CENTER -1

typedef struct joystate_s {
    int             numAxes;        // Number of axes present.
    int             numButtons;     // Number of buttons present.
    int             numHats;        // Number of hats present.
    int             axis[IJOY_MAXAXES];
    int             buttonDowns[IJOY_MAXBUTTONS]; // Button down count.
    int             buttonUps[IJOY_MAXBUTTONS]; // Button up count.
    float           hatAngle[IJOY_MAXHATS];    // 0 - 359 degrees.
} joystate_t;

void Joystick_Register(void);

boolean Joystick_Init(void);

void Joystick_Shutdown(void);

boolean Joystick_IsPresent(void);

void Joystick_GetState(joystate_t* state);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDENG_SYSTEM_JOYSTICK_H
