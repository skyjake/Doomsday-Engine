/** @file sys_input.cpp  Keyboard and mouse input pre-processing.
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

#include "de_platform.h"
#include "ui/sys_input.h"

#include <cstdlib>
#include <de/c_wrapper.h>

#include "sys_system.h"

// #include "ui/mouse_qt.h"  // portable
// #ifdef WIN32
// #  include "directinput.h"
// #  include "mouse_win32.h"
// #endif

#define EVBUFSIZE       64
#define KEYBUFSIZE      32

static dd_bool initOk;
//static byte useMouse;  ///< Input enabled from mouse?
//static mouseinterface_t *iMouse; ///< Current mouse interface.

static keyevent_t keyEvents[EVBUFSIZE];
static int evHead, evTail;

void I_Register(void)
{
#ifdef __CLIENT__
    Joystick_Register();
#endif
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

    if (evHead == evTail)
        return NULL;            // No more...
    ev = keyEvents + evTail;
    evTail = (evTail + 1) % EVBUFSIZE;
    return ev;
}

//static void Mouse_Init(void)
//{
//    if (CommandLine_Check("-nomouse") || novideo)
//        return;

//    LOG_AS("Mouse_Init");

//    DE_ASSERT(iMouse);
//    iMouse->init();

    // Init was successful.
//    useMouse = true;
//}

dd_bool I_InitInterfaces(void)
{
    if (initOk)
        return true; // Already initialized.

#ifdef __CLIENT__

    // Select drivers.
//    iMouse = &qtMouse;
#ifdef WIN32
//    iMouse = &win32Mouse;
//    DirectInput_Init();
#endif

//    Mouse_Init();
    Joystick_Init();

#endif // __CLIENT__

    initOk = true;
    return true;
}

void I_ShutdownInterfaces()
{
    if (!initOk)
        return; // Not initialized.

#ifdef __CLIENT__
//    if (useMouse) iMouse->shutdown();
//    useMouse = false;

    Joystick_Shutdown();
# ifdef WIN32
//    DirectInput_Shutdown();
# endif
#endif

    initOk = false;
}

void Keyboard_Submit(int type, int ddKey, int native, const char* text)
{
    if (ddKey != 0)
    {
        keyevent_t* e = newKeyEvent();
        e->type = type;
        e->ddkey = ddKey;
        e->native = native;
        if (text)
        {
            strncpy(e->text, text, sizeof(e->text) - 1);
        }
    }
}

size_t Keyboard_GetEvents(keyevent_t *evbuf, size_t bufsize)
{
    keyevent_t *e;
    size_t      i = 0;

    if (!initOk)
        return 0;

    // Get the events.
    for (i = 0; i < bufsize; ++i)
    {
        e = getKeyEvent();
        if (!e) break; // No more events.
        memcpy(&evbuf[i], e, sizeof(*e));
    }

    return i;
}

#if 0
dd_bool Mouse_IsPresent(void)
{
    //if (!initOk) I_InitInterfaces();
    return useMouse;
}

void Mouse_Poll(void)
{
    if (useMouse)
    {
//        iMouse->poll();
    }
}

void Mouse_GetState(mousestate_t *state)
{
    if (useMouse)
    {
//        iMouse->getState(state);
    }
}

void Mouse_Trap(dd_bool enabled)
{
    if (useMouse)
    {
//        iMouse->trap(enabled);
    }
}
#endif
