/**
 * @file sys_input.c
 * Keyboard and mouse input pre-processing. @ingroup input
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_misc.h"

#define EVBUFSIZE       64
#define KEYBUFSIZE      32

typedef struct clicker_s {
    int down;                   // Count for down events.
    int up;                     // Count for up events.
} clicker_t;

static boolean initIOk;
static byte useMouse; // Input enabled from a source?

static keyevent_t keyEvents[EVBUFSIZE];
static int evHead, evTail;

static clicker_t mouseClickers[IMB_MAXBUTTONS];

void I_Register(void)
{
    Joystick_Register();
}

static keyevent_t *newKeyEvent(void)
{
    keyevent_t *ev = keyEvents + evHead;

    evHead = (evHead + 1) % EVBUFSIZE;
    memset(ev, 0, sizeof(*ev));
    return ev;
}

/**
 * @return The oldest event from the buffer.
 */
static keyevent_t *getKeyEvent(void)
{
    keyevent_t *ev;

    if(evHead == evTail)
        return NULL;            // No more...
    ev = keyEvents + evTail;
    evTail = (evTail + 1) % EVBUFSIZE;
    return ev;
}

#if 0 // move to keycode.c
/**
 * Translate the SDL symbolic key code to a DDKEY.
 * \fixme A translation array for these?
 */
int translateKeyCode(SDLKey sym)
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
#endif

#if 0
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
#endif

static void Mouse_Init(void)
{
    if(ArgCheck("-nomouse") || novideo)
        return;

    // Init was successful.
    useMouse = true;
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

    Mouse_Init();
    Joystick_Init();
    initIOk = true;
    return true;
}

void I_Shutdown(void)
{
    if(!initIOk)
        return; // Not initialized.

    Joystick_Shutdown();
    initIOk = false;
}

void Keyboard_Submit(int type, int ddKey, const char* text)
{
    keyevent_t* e = newKeyEvent();
    e->type = type;
    e->ddkey = ddKey;
    if(text)
    {
        strncpy(e->text, text, sizeof(e->text) - 1);
    }
}

size_t Keyboard_GetEvents(keyevent_t *evbuf, size_t bufsize)
{
    keyevent_t *e;
    size_t      i = 0;

    if(!initIOk)
        return 0;

    // Get the events.
    for(i = 0; i < bufsize; ++i)
    {
        e = getKeyEvent();
        if(!e) break; // No more events.
        memcpy(&evbuf[i], e, sizeof(*e));
    }

    return i;
}

boolean Mouse_IsPresent(void)
{
    return useMouse;
}

void Mouse_GetState(mousestate_t *state)
{
    //byte        buttons;
    int         i;

    memset(state, 0, sizeof(*state));

    // Has the mouse been initialized?
    if(!Mouse_IsPresent() || !initIOk)
        return;

    // Cursor movement or position.

    /*
    buttons = SDL_GetRelativeMouseState(&state->x, &state->y);

    // Ignore the first nonzero offset, it appears it's a nasty jump.
    if(!gotFirstMouseMove && (state->x || state->y))
    {
        gotFirstMouseMove = true;
        state->x = state->y = 0;
    }
    */

    // Button presses and releases.
    for(i = 0; i < IMB_MAXBUTTONS; ++i)
    {
        state->buttonDowns[i] = mouseClickers[i].down;
        state->buttonUps[i] = mouseClickers[i].up;

        // Reset counters.
        mouseClickers[i].down = mouseClickers[i].up = 0;
    }

    // Wheel actions.
}
