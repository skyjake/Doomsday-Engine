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

#ifdef WIN32
#  include "directinput.h"
#  include "mouse_win32.h"
#endif

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_misc.h"

#include "mouse_qt.h" // portable

#define EVBUFSIZE       64
#define KEYBUFSIZE      32

static boolean initOk;
static byte useMouse; // Input enabled from mouse?
static mouseinterface_t* iMouse; ///< Current mouse interface.

static keyevent_t keyEvents[EVBUFSIZE];
static int evHead, evTail;

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
            e->event = (event.type == SDL_KEYDOWN ? IKE_DOWN : IKE_UP);
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
    if(CommandLine_Check("-nomouse") || novideo)
        return;

    assert(iMouse);
    iMouse->init();

    // Init was successful.
    useMouse = true;
}

boolean I_Init(void)
{
    if(initOk)
        return true; // Already initialized.

    // Select drivers.
    iMouse = &qtMouse;
#ifdef WIN32
    iMouse = &win32Mouse;
    DirectInput_Init();
#endif

    Mouse_Init();
    Joystick_Init();

    initOk = true;
    return true;
}

void I_Shutdown(void)
{
    if(!initOk)
        return; // Not initialized.

    if(useMouse) iMouse->shutdown();
    useMouse = false;

    Joystick_Shutdown();
    initOk = false;

#ifdef WIN32
    DirectInput_Shutdown();
#endif
}

void Keyboard_Submit(int type, int ddKey, int native, const char* text)
{
    if(ddKey != 0)
    {
        keyevent_t* e = newKeyEvent();
        e->type = type;
        e->ddkey = ddKey;
        e->native = native;
        if(text)
        {
            strncpy(e->text, text, sizeof(e->text) - 1);
        }
    }
}

size_t Keyboard_GetEvents(keyevent_t *evbuf, size_t bufsize)
{
    keyevent_t *e;
    size_t      i = 0;

    if(!initOk)
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
    if(!initOk) I_Init();
    return useMouse;
}

void Mouse_Poll(void)
{
    if(useMouse)
    {
        iMouse->poll();
    }
}

void Mouse_GetState(mousestate_t *state)
{
    if(useMouse)
    {
        iMouse->getState(state);
    }
}

void Mouse_Trap(boolean enabled)
{
    if(useMouse)
    {
        iMouse->trap(enabled);
    }
}
