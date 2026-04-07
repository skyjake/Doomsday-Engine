/** @file joystick.cpp  SDL Joystick input pre-processing for Unix.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <yagisan@dengine.net>
 * @authors Copyright © 2005 Zachary Keene <zjkeene@bellsouth.net>
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

#include <stdlib.h>
#include <SDL.h>
#undef main

#include "de_base.h"
#include "ui/joystick.h"

#include <doomsday/console/var.h>

#define CONVCONST       ((IJOY_AXISMAX - IJOY_AXISMIN) / 65535.0)

int     joydevice = 0;          // Joystick index to use (cvar)
byte    useJoystickCvar = true; // Joystick input enabled? (cvar)

static dd_bool joyInited;
static byte    joyAvailable; // Input enabled from a source?
static dd_bool joyButtonWasDown[IJOY_MAXBUTTONS];

static SDL_Joystick *joy;

void Joystick_Register(void)
{
#ifndef DE_NO_SDL
    C_VAR_INT("input-joy-device", &joydevice, CVF_NO_MAX | CVF_PROTECTED, 0, 0);
    C_VAR_BYTE("input-joy", &useJoystickCvar, 0, 0, 1);
#endif
}

#ifndef DE_NO_SDL
static void initialize(void)
{
    int joycount;

    if (isDedicated || CommandLine_Check("-nojoy"))
        return;

#ifdef SOLARIS
    /**
     * @note  Solaris has no Joystick support according to
     * https://sourceforge.net/tracker/?func=detail&atid=542099&aid=1732554&group_id=74815
     */
    return;
#endif

    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK))
    {
        LOG_INPUT_ERROR("SDL init failed for joystick: %s") << SDL_GetError();
    }

    if ((joycount = SDL_NumJoysticks()) > 0)
    {
        if (joydevice > joycount)
        {
            LOG_INPUT_WARNING("Using the default joystick instead of joystick #%i") << joydevice;
            joy = SDL_JoystickOpen(0);
        }
        else
            joy = SDL_JoystickOpen(joydevice);
    }

    if (joy)
    {
        // Show some info.
        LOG_INPUT_MSG("Joystick name: %s" ) << Joystick_Name();

        // We'll handle joystick events manually
        SDL_JoystickEventState(SDL_ENABLE);

        LOG_INPUT_VERBOSE("Joystick reports %i axes, %i buttons, %i hats, and %i trackballs")
                << SDL_JoystickNumAxes(joy)
                << SDL_JoystickNumButtons(joy)
                << SDL_JoystickNumHats(joy)
                << SDL_JoystickNumBalls(joy);

        joyAvailable = true;
    }
    else
    {
        LOG_INPUT_NOTE("No joysticks found");
        joyAvailable = false;
    }
}
#endif

bool Joystick_Init(void)
{
#ifndef DE_NO_SDL
    if (joyInited) return true; // Already initialized.

    LOG_AS("Joystick_Init");

    initialize();
    joyInited = true;
#endif
    return true;
}

void Joystick_Shutdown(void)
{
#ifndef DE_NO_SDL
    if (!joyInited) return; // Not initialized.

    if (joy)
    {
        SDL_JoystickClose(joy);
        joy = 0;
    }

    joyInited = false;
#endif
}

bool Joystick_IsPresent(void)
{
    return joyAvailable;
}

void Joystick_GetState(joystate_t *state)
{
#ifndef DE_NO_SDL
    int         i, pov;

    memset(state, 0, sizeof(*state));

    // Initialization has not been done.
    if (!Joystick_IsPresent() || !useJoystickCvar || !joyInited)
        return;

    // Update joysticks
    SDL_JoystickUpdate();

    // What do we have available to us?
    state->numAxes =    MIN_OF( SDL_JoystickNumAxes(joy),    IJOY_MAXAXES );
    state->numButtons = MIN_OF( SDL_JoystickNumButtons(joy), IJOY_MAXBUTTONS );
    state->numHats =    MIN_OF( SDL_JoystickNumHats(joy),    IJOY_MAXHATS );

    for (i = 0; i < state->numAxes; ++i)
    {
        int value = SDL_JoystickGetAxis(joy, i);
        // SDL returns a value between -32768 and 32767, but Doomsday is expecting
        // -10000 to 10000. We'll convert as we go.
        value = ((value + 32768) * CONVCONST) + IJOY_AXISMIN;
        state->axis[i] = value;
    }
    for (i = 0; i < state->numButtons; ++i)
    {
        int isDown = SDL_JoystickGetButton(joy, i);

        if (isDown && !joyButtonWasDown[i])
        {
            state->buttonDowns[i] = 1;
            state->buttonUps[i] = 0;
        }
        else if (!isDown && joyButtonWasDown[i])
        {
            state->buttonDowns[i] = 0;
            state->buttonUps[i] = 1;
        }

        joyButtonWasDown[i] = isDown;
    }
    for (i = 0; i < state->numHats; ++i)
    {
        pov = SDL_JoystickGetHat(joy, i);
        switch (pov)
        {
            case SDL_HAT_UP:
                state->hatAngle[i] = 0;
                break;

            case SDL_HAT_RIGHT:
                state->hatAngle[i] = 90;
                break;

            case SDL_HAT_DOWN:
                state->hatAngle[i] = 180;
                break;

            case SDL_HAT_LEFT:
                state->hatAngle[i] = 270;
                break;

            case SDL_HAT_RIGHTUP:
                state->hatAngle[i] = 45;
                break;

            case SDL_HAT_RIGHTDOWN:
                state->hatAngle[i] = 135;
                break;

            case SDL_HAT_LEFTUP:
                state->hatAngle[i] = 315;
                break;

            case SDL_HAT_LEFTDOWN:
                state->hatAngle[i] = 225;
                break;

            default:
                state->hatAngle[i] = IJOY_POV_CENTER;
        }
    }
#else
    memset(state, 0, sizeof(*state));
#endif
}

de::String Joystick_Name()
{
#ifndef DE_NO_SDL
    const char *joyName = SDL_JoystickName(joy);
    return joyName ? joyName : "";
#else
    return "";
#endif
}
