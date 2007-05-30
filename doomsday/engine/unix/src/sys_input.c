/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <yagisan@dengine.net>
 *\author Copyright © 2005 Zachary Keene <zjkeene@bellsouth.net>
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
 * sys_input.c: Keyboard, mouse and joystick input using SDL
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <SDL.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#define EVBUFSIZE       64

#define KEYBUFSIZE      32

#define CONVCONST       ((IJOY_AXISMAX - IJOY_AXISMIN) / 65535.0)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean novideo;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     joydevice = 0;          // Joystick index to use.
byte    usejoystick = false;    // Joystick input enabled?

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean initIOk;
static byte useMouse, useJoystick;

static keyevent_t keyEvents[EVBUFSIZE];
static int evHead, evTail;

static int wheelCount;

static SDL_Joystick *joy;
static int numbuttons, numaxes;

// CODE --------------------------------------------------------------------

void I_Register(void)
{
    C_VAR_INT("input-joy-device", &joydevice, CVF_NO_MAX | CVF_PROTECTED, 0, 0);
    C_VAR_BYTE("input-joy", &usejoystick, 0, 0, 1);
}

/**
 * @return          A new key event struct from the buffer.
 */
keyevent_t *I_NewKeyEvent(void)
{
    keyevent_t *ev = keyEvents + evHead;

    evHead = (evHead + 1) % EVBUFSIZE;
    memset(ev, 0, sizeof(*ev));
    return ev;
}

/**
 * @return          The oldest event from the buffer.
 */
keyevent_t *I_GetKeyEvent(void)
{
    keyevent_t *ev;

    if(evHead == evTail)
        return NULL;            // No more...
    ev = keyEvents + evTail;
    evTail = (evTail + 1) % EVBUFSIZE;
    return ev;
}

/**
 * Translate the SDL symbolic key code to a DDKEY.
 * \fixme A translation array for these?
 */
int I_TranslateKeyCode(SDLKey sym)
{
    switch(sym)
    {
    case 167:                   // Tilde
        return 96;              // ASCII: '`'

    case '\b':                  // Backspace
        return DDKEY_BACKSPACE;

    case SDLK_PAUSE:
        return DDKEY_PAUSE;

    case SDLK_UP:
        return DDKEY_UPARROW;

    case SDLK_DOWN:
        return DDKEY_DOWNARROW;

    case SDLK_LEFT:
        return DDKEY_LEFTARROW;

    case SDLK_RIGHT:
        return DDKEY_RIGHTARROW;

    case SDLK_RSHIFT:
    case SDLK_LSHIFT:
        return DDKEY_RSHIFT;

    case SDLK_RALT:
    case SDLK_LALT:
        return DDKEY_RALT;

    case SDLK_RCTRL:
    case SDLK_LCTRL:
        return DDKEY_RCTRL;

    case SDLK_F1:
        return DDKEY_F1;

    case SDLK_F2:
        return DDKEY_F2;

    case SDLK_F3:
        return DDKEY_F3;

    case SDLK_F4:
        return DDKEY_F4;

    case SDLK_F5:
        return DDKEY_F5;

    case SDLK_F6:
        return DDKEY_F6;

    case SDLK_F7:
        return DDKEY_F7;

    case SDLK_F8:
        return DDKEY_F8;

    case SDLK_F9:
        return DDKEY_F9;

    case SDLK_F10:
        return DDKEY_F10;

    case SDLK_F11:
        return DDKEY_F11;

    case SDLK_F12:
        return DDKEY_F12;

    case SDLK_NUMLOCK:
        return DDKEY_NUMLOCK;

    case SDLK_SCROLLOCK:
        return DDKEY_SCROLL;

    case SDLK_KP0:
        return DDKEY_NUMPAD0;

    case SDLK_KP1:
        return DDKEY_NUMPAD1;

    case SDLK_KP2:
        return DDKEY_NUMPAD2;

    case SDLK_KP3:
        return DDKEY_NUMPAD3;

    case SDLK_KP4:
        return DDKEY_NUMPAD4;

    case SDLK_KP5:
        return DDKEY_NUMPAD5;

    case SDLK_KP6:
        return DDKEY_NUMPAD6;

    case SDLK_KP7:
        return DDKEY_NUMPAD7;

    case SDLK_KP8:
        return DDKEY_NUMPAD8;

    case SDLK_KP9:
        return DDKEY_NUMPAD9;

    case SDLK_KP_PERIOD:
        return DDKEY_DECIMAL;

    case SDLK_KP_PLUS:
        return DDKEY_ADD;

    case SDLK_KP_MINUS:
        return DDKEY_SUBTRACT;

    case SDLK_KP_DIVIDE:
        return '/';

    case SDLK_KP_MULTIPLY:
        return '*';

    case SDLK_KP_ENTER:
        return DDKEY_ENTER;

    case SDLK_INSERT:
        return DDKEY_INS;

    case SDLK_DELETE:
        return DDKEY_DEL;

    case SDLK_HOME:
        return DDKEY_HOME;

    case SDLK_END:
        return DDKEY_END;

    case SDLK_PAGEUP:
        return DDKEY_PGUP;

    case SDLK_PAGEDOWN:
        return DDKEY_PGDN;

    default:
        break;
    }
    return sym;
}

/**
 * SDL's events are all returned from the same routine.  This function
 * is called periodically, and the events we are interested in a saved
 * into our own buffer.
 */
void I_PollEvents(void)
{
    SDL_Event   event;
    keyevent_t *e;

    while(SDL_PollEvent(&event))
    {
        switch(event.type)
        {
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            e = I_NewKeyEvent();
            e->event = (event.type == SDL_KEYDOWN ? IKE_KEY_DOWN : IKE_KEY_UP);
            e->code = I_TranslateKeyCode(event.key.keysym.sym);
            /*printf("sdl:%i code:%i\n", event.key.keysym.scancode, e->code);*/
            break;

        case SDL_MOUSEBUTTONDOWN:
            if(event.button.button == SDL_BUTTON_WHEELUP)
                wheelCount++;
            if(event.button.button == SDL_BUTTON_WHEELDOWN)
                wheelCount--;
            break;

        case SDL_QUIT:
            // The system wishes to close the program immediately...
            Sys_Quit();
            break;

        default:
            // The rest of the events are ignored.
            break;
        }
    }
}

void I_InitMouse(void)
{
    if(ArgCheck("-nomouse") || novideo)
        return;

    // Init was successful.
    useMouse = true;

    // Grab all input.
    SDL_WM_GrabInput(SDL_GRAB_ON);
}

void I_InitJoystick(void)
{
    int         joycount;

    if(ArgCheck("-nojoy"))
        return;

    if((joycount = SDL_NumJoysticks()) > 0)
    {
        if(joydevice > joycount)
        {
            Con_Message("I_InitJoystick: joydevice = %i, out of range.\n",
                        joydevice);
            joy = SDL_JoystickOpen(0);
        }
        else
            joy = SDL_JoystickOpen(joydevice);
    }

    if(joy)
    {
        // Show some info.
        Con_Message("I_InitJoystick: %s\n", SDL_JoystickName(SDL_JoystickIndex(joy)));

        // We'll handle joystick events manually
        SDL_JoystickEventState(SDL_IGNORE);

        numaxes = SDL_JoystickNumAxes(joy);
        numbuttons = SDL_JoystickNumButtons(joy);
        if(numbuttons > IJOY_MAXBUTTONS)
            numbuttons = IJOY_MAXBUTTONS;

        useJoystick = true;
    }
    else
    {
        Con_Message("I_InitJoystick: No joysticks found\n");
        useJoystick = false;
    }
}

/**
 * Initialize input.
 *
 * @return              <code>true</code> if successful.
 */
int I_Init(void)
{
    if(initIOk)
        return true; // Already initialized.

    I_InitMouse();
    I_InitJoystick();
    initIOk = true;
    return true;
}

void I_Shutdown(void)
{
    if(!initIOk)
        return; // Not initialized.
    if (joy) SDL_JoystickClose(joy);
    initIOk = false;
}

boolean I_MousePresent(void)
{
    return useMouse;
}

boolean I_JoystickPresent(void)
{
    return useJoystick;
}

int I_GetKeyEvents(keyevent_t *evbuf, int bufsize)
{
    keyevent_t *e;
    int         i = 0;

    if(!initIOk)
        return 0;

    // Get new events from SDL.
    I_PollEvents();

    // Get the events.
    for(i = 0; i < bufsize; ++i)
    {
        e = I_GetKeyEvent();
        if(!e)
            break; // No more events.
        memcpy(&evbuf[i], e, sizeof(*e));
    }

    return i;
}

void I_GetMouseState(mousestate_t *state)
{
    Uint8       buttons;
    int         i;

    memset(state, 0, sizeof(*state));

    // Has the mouse been initialized?
    if(!I_MousePresent() || !initIOk)
        return;

    buttons = SDL_GetRelativeMouseState(&state->x, &state->y);

    if(buttons & SDL_BUTTON(SDL_BUTTON_LEFT))
        state->buttons |= IMB_LEFT;
    if(buttons & SDL_BUTTON(SDL_BUTTON_RIGHT))
        state->buttons |= IMB_RIGHT;
    if(buttons & SDL_BUTTON(SDL_BUTTON_MIDDLE))
        state->buttons |= IMB_MIDDLE;

    // The buttons bitfield is ordered according to the numbering.
    for(i = 4; i < 8; ++i)
    {
        if(buttons & SDL_BUTTON(i))
            state->buttons |= 1 << (i - 1);
    }

    state->z = wheelCount * 20;
    wheelCount = 0;
}

void I_GetJoystickState(joystate_t *state)
{
    int         i, pov;

    memset(state, 0, sizeof(*state));

    // Initialization has not been done.
    if(!I_JoystickPresent() || !usejoystick || !initIOk)
        return;

    // Update joysticks
    SDL_JoystickUpdate();

    /** Grab the first three axes. SDL returns a value between -32768 and
    * 32767, but Doomsday is expecting -10000 to 10000. We'll convert
    * as we go.
    *
    * \fixme would changing IJOY_AXISMIN and IJOY_AXISMAX to -32768 and
    * 32767 break the Windows version? If not that would make this
    * cleaner.
    */
    for(i = 0; i < 3; ++i)
    {
        int         value;
        if(i > numaxes)
            break;

        value = SDL_JoystickGetAxis(joy, i);
        value = ((value + 32768) * CONVCONST) + IJOY_AXISMIN;

        state->axis[i] = value;
    }

    // Dunno what to do with these using SDL so we'll set them
    // all to 0 for now.
    state->rotAxis[0] = 0;
    state->rotAxis[1] = 0;
    state->rotAxis[2] = 0;

    state->slider[0] = 0;
    state->slider[1] = 0;

    for(i = 0; i < numbuttons; ++i)
        state->buttons[i] = SDL_JoystickGetButton(joy, i);

    pov = SDL_JoystickGetHat(joy, 0);
    switch(pov)
    {
    case SDL_HAT_UP:
        state->povAngle = 0;
        break;

    case SDL_HAT_RIGHT:
        state->povAngle = 90;
        break;

    case SDL_HAT_DOWN:
        state->povAngle = 180;
        break;

    case SDL_HAT_LEFT:
        state->povAngle = 270;
        break;

    case SDL_HAT_RIGHTUP:
        state->povAngle = 45;
        break;

    case SDL_HAT_RIGHTDOWN:
        state->povAngle = 135;
        break;

    case SDL_HAT_LEFTUP:
        state->povAngle = 315;
        break;

    case SDL_HAT_LEFTDOWN:
        state->povAngle = 225;
        break;

    default:
        state->povAngle = IJOY_POV_CENTER;
    }
}
