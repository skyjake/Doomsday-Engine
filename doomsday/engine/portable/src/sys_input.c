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

static boolean initOk;
static byte useMouse; // Input enabled from a source?

static keyevent_t keyEvents[EVBUFSIZE];
static int evHead, evTail;

static int mousePosX, mousePosY; // Window position.
static struct { int dx, dy; } mouseDelta[IMA_MAXAXES];
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
    if(ArgCheck("-nomouse") || novideo)
        return;

    memset(&mouseDelta, 0, sizeof(mouseDelta));
    memset(&mouseClickers, 0, sizeof(mouseClickers));

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
    if(initOk)
        return true; // Already initialized.

    Mouse_Init();
    Joystick_Init();

    initOk = true;
    return true;
}

void I_Shutdown(void)
{
    if(!initOk)
        return; // Not initialized.

    Joystick_Shutdown();
    initOk = false;
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
    return useMouse;
}

void Mouse_SubmitButton(int button, boolean isDown)
{
    if(button < 0 || button >= IMB_MAXBUTTONS) return; // Ignore...

    if(isDown)
        mouseClickers[button].down++;
    else
        mouseClickers[button].up++;
}

void Mouse_SubmitMotion(int axis, int deltaX, int deltaY)
{
    if(axis < 0 || axis >= IMA_MAXAXES) return; // Ignore...

    /// @todo It would likely be better to directly post a ddevent out of this.

    mouseDelta[axis].dx += deltaX;
    mouseDelta[axis].dy += deltaY;
}

void Mouse_SubmitWindowPosition(int x, int y)
{
    // Absolute coordintes.
    mouseDelta[IMA_POINTER].dx = x;
    mouseDelta[IMA_POINTER].dy = y;
}

void Mouse_GetState(mousestate_t *state)
{
    int i;

    memset(state, 0, sizeof(*state));

    // Has the mouse been initialized?
    if(!Mouse_IsPresent() || !initOk)
        return;

    // Position and wheel.
    for(i = 0; i < IMA_MAXAXES; ++i)
    {
        state->axis[i].x = mouseDelta[i].dx;
        state->axis[i].y = mouseDelta[i].dy;

        // Reset.
        mouseDelta[i].dx = mouseDelta[i].dy = 0;
    }

    // Button presses and releases.
    for(i = 0; i < IMB_MAXBUTTONS; ++i)
    {
        state->buttonDowns[i] = mouseClickers[i].down;
        state->buttonUps[i] = mouseClickers[i].up;

        // Reset counters.
        mouseClickers[i].down = mouseClickers[i].up = 0;
    }    
}
