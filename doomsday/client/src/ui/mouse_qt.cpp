/** @file mouse_qt.cpp
 *
 * Mouse driver that gets mouse input from the Qt based canvas widget.
 * @ingroup input
 *
 * @authors Copyright © 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "con_main.h"
#include "ui/sys_input.h"
#include "ui/window.h"
#include "ui/mouse_qt.h"
#include "ui/canvas.h"
#include <string.h>

#include <QWidget>
#include <QPoint>
#include <QCursor>

typedef struct clicker_s {
    int down;                   // Count for down events.
    int up;                     // Count for up events.
} clicker_t;

//static int mousePosX, mousePosY; // Window position.
static struct { int dx, dy; } mouseDelta[IMA_MAXAXES];
static clicker_t mouseClickers[IMB_MAXBUTTONS];
static bool mouseTrapped = false;
static QPoint prevMousePos;

static int Mouse_Qt_Init(void)
{
    memset(&mouseDelta, 0, sizeof(mouseDelta));
    memset(&mouseClickers, 0, sizeof(mouseClickers));
    mouseTrapped = false;
    prevMousePos = QPoint();
    return true;
}

static void Mouse_Qt_Shutdown(void)
{
    // nothing to do
}

static void Mouse_Qt_Poll()
{
    if(!mouseTrapped) return;

#ifndef LIBDENG_CANVAS_TRACK_WITH_MOUSE_MOVE_EVENTS
    Window *mainWindow = Window::main();
    DENG_ASSERT(mainWindow);
    QWidget *widget = mainWindow->widget();
    if(!widget) return; // Hmm?

    QPoint curPos = widget->mapFromGlobal(QCursor::pos());
    if(!prevMousePos.isNull())
    {
        QPoint delta = curPos - prevMousePos;
        if(!delta.isNull())
        {
            Mouse_Qt_SubmitMotion(IMA_POINTER, delta.x(), delta.y());

            // Keep the cursor centered.
            QPoint mid(mainWindow->width() / 2, mainWindow->height() / 2);
            QCursor::setPos(widget->mapToGlobal(mid));
            prevMousePos = mid;
        }
    }
    else
    {
        prevMousePos = curPos;
    }
#endif
}

static void Mouse_Qt_GetState(mousestate_t *state)
{
    int i;

    memset(state, 0, sizeof(*state));

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

static void Mouse_Qt_Trap(boolean enabled)
{
    mouseTrapped = enabled;
    prevMousePos = QPoint();
}

void Mouse_Qt_SubmitButton(int button, boolean isDown)
{
    if(button < 0 || button >= IMB_MAXBUTTONS) return; // Ignore...

    if(isDown)
        mouseClickers[button].down++;
    else
        mouseClickers[button].up++;
}

void Mouse_Qt_SubmitMotion(int axis, int deltaX, int deltaY)
{
    if(axis < 0 || axis >= IMA_MAXAXES) return; // Ignore...

    /// @todo It would likely be better to directly post a ddevent out of this.

    if(axis == IMA_WHEEL)
    {
        int idx = ( deltaX < 0? IMB_MWHEELLEFT
                  : deltaX > 0? IMB_MWHEELRIGHT
                  : deltaY < 0? IMB_MWHEELUP
                              : IMB_MWHEELDOWN);

        // We are not yet equipped to handle finer wheel motions.
        Mouse_Qt_SubmitButton(idx, true);
        Mouse_Qt_SubmitButton(idx, false);
        return;
    }

    mouseDelta[axis].dx += deltaX;
    mouseDelta[axis].dy += deltaY;
}

void Mouse_Qt_SubmitWindowPosition(int x, int y)
{
    // Absolute coordintes.
    mouseDelta[IMA_POINTER].dx = x;
    mouseDelta[IMA_POINTER].dy = y;
}

// The global interface.
mouseinterface_t qtMouse = {
    Mouse_Qt_Init,
    Mouse_Qt_Shutdown,
    Mouse_Qt_Poll,
    Mouse_Qt_GetState,
    Mouse_Qt_Trap
};
