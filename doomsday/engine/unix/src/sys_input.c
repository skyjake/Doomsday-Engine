/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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


/**
 * sys_input.c: Keyboard, mouse and joystick input using SDL
 * \todo - Unify this with Win32.
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

typedef struct clicker_s {
    int down;                   // Count for down events.
    int up;                     // Count for up events.
} clicker_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int novideo;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     joydevice = 0;          // Joystick index to use.
byte    usejoystick = false;    // Joystick input enabled?

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean initIOk;
static byte useMouse, useJoystick;

static keyevent_t keyEvents[EVBUFSIZE];
static int evHead, evTail;

static clicker_t mouseClickers[IMB_MAXBUTTONS];
static clicker_t joyClickers[IJOY_MAXBUTTONS];
static boolean gotFirstMouseMove = false;

static SDL_Joystick *joy;

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

    case SDLK_RETURN:
        return DDKEY_RETURN;

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
        return DDKEY_DIVIDE;

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

    case SDLK_PRINT:
        return DDKEY_PRINT;

    case SDLK_CAPSLOCK:
        return DDKEY_CAPSLOCK;

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
            e->ddkey = I_TranslateKeyCode(event.key.keysym.sym);
            /*printf("sdl:%i ddkey:%i\n", event.key.keysym.scancode, e->ddkey);*/
            break;

        case SDL_MOUSEBUTTONDOWN:
            mouseClickers[MIN_OF(event.button.button - 1, IMB_MAXBUTTONS - 1)].down++;
            break;

        case SDL_MOUSEBUTTONUP:
            mouseClickers[MIN_OF(event.button.button - 1, IMB_MAXBUTTONS - 1)].up++;
            break;

        case SDL_JOYBUTTONDOWN:
            joyClickers[MIN_OF(event.jbutton.button, IJOY_MAXBUTTONS - 1)].down++;
            break;

        case SDL_JOYBUTTONUP:
            joyClickers[MIN_OF(event.jbutton.button, IJOY_MAXBUTTONS - 1)].up++;
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
    gotFirstMouseMove = false;

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
        SDL_JoystickEventState(SDL_ENABLE);

        if(verbose)
        {
            Con_Message("I_InitJoystick: Joystick reports %i axes, %i buttons, %i hats, "
                        "and %i trackballs.\n",
                        SDL_JoystickNumAxes(joy),
                        SDL_JoystickNumButtons(joy),
                        SDL_JoystickNumHats(joy),
                        SDL_JoystickNumBalls(joy));
        }

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
 * @return              @c true, if successful.
 */
boolean I_Init(void)
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

size_t I_GetKeyEvents(keyevent_t *evbuf, size_t bufsize)
{
    keyevent_t *e;
    size_t      i = 0;

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

    // Ignore the first nonzero offset, it appears it's a nasty jump.
    if(!gotFirstMouseMove && (state->x || state->y))
    {
        gotFirstMouseMove = true;
        state->x = state->y = 0;
    }

    for(i = 0; i < IMB_MAXBUTTONS; ++i)
    {
        state->buttonDowns[i] = mouseClickers[i].down;
        state->buttonUps[i] = mouseClickers[i].up;

        // Reset counters.
        mouseClickers[i].down = mouseClickers[i].up = 0;
    }
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

    // What do we have available to us?
    state->numAxes =    MIN_OF( SDL_JoystickNumAxes(joy),    IJOY_MAXAXES );
    state->numButtons = MIN_OF( SDL_JoystickNumButtons(joy), IJOY_MAXBUTTONS );
    state->numHats =    MIN_OF( SDL_JoystickNumHats(joy),    IJOY_MAXHATS );

    for(i = 0; i < state->numAxes; ++i)
    {
        int value = SDL_JoystickGetAxis(joy, i);
        // SDL returns a value between -32768 and 32767, but Doomsday is expecting
        // -10000 to 10000. We'll convert as we go.
        value = ((value + 32768) * CONVCONST) + IJOY_AXISMIN;
        state->axis[i] = value;
    }
    for(i = 0; i < state->numButtons; ++i)
    {
        state->buttonDowns[i] = joyClickers[i].down;
        state->buttonUps[i] = joyClickers[i].up;

        // Reset counters.
        joyClickers[i].down = joyClickers[i].up = 0;
    }
    for(i = 0; i < state->numHats; ++i)
    {
        pov = SDL_JoystickGetHat(joy, i);

        /*
        {
            // Debug: Simulating the hat with buttons 1-4.
            int buts[4] = {
                SDL_JoystickGetButton(joy, 0),
                SDL_JoystickGetButton(joy, 1),
                SDL_JoystickGetButton(joy, 2),
                SDL_JoystickGetButton(joy, 3)
            };
            pov = SDL_HAT_CENTERED;
            if(buts[0] && !buts[1] && !buts[3])
                pov = SDL_HAT_UP;
            else if(buts[0] && buts[1])
                pov = SDL_HAT_RIGHTUP;
            else if(buts[1] && !buts[2])
                pov = SDL_HAT_RIGHT;
            else if(buts[1] && buts[2])
                pov = SDL_HAT_RIGHTDOWN;
            else if(buts[2] && !buts[3])
                pov = SDL_HAT_DOWN;
            else if(buts[2] && buts[3])
                pov = SDL_HAT_LEFTDOWN;
            else if(buts[3] && !buts[0])
                pov = SDL_HAT_LEFT;
            else if(buts[3] && buts[0])
                pov = SDL_HAT_LEFTUP;
        }
         */

        switch(pov)
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
}
