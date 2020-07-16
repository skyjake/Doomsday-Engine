/** @file guiloop.cpp
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/guiloop.h"
#include "de/glwindow.h"
#include "de/eventloop.h"
#include "de/coreevent.h"
#include "de/windowsystem.h"

namespace de {

DE_PIMPL(GuiLoop)
, DE_OBSERVES(GLWindow, Swap)
{
    GLWindow *window = nullptr;

    Impl(Public *i) : Base(i)
    {}

    void windowSwapped(GLWindow &window) override
    {
        // Always do a loop iteration after a frame is complete.
        if (&window == &GLWindow::getMain())
        {
            EventLoop::post(new CoreEvent([this]() { self().nextLoopIteration(); }));
        }
    }
};

GuiLoop::GuiLoop()
    : d(new Impl(this))
{
    // Make sure some events get triggered even though the window refresh is not (yet) running.
    setRate(10);
}

void GuiLoop::setWindow(GLWindow *window)
{
    if (d->window) d->window->audienceForSwap() -= d;
    d->window = window;
    if (d->window) d->window->audienceForSwap() += d;
}

GuiLoop &GuiLoop::get() // static
{
    return static_cast<GuiLoop &>(Loop::get());
}

void GuiLoop::nextLoopIteration()
{
    WindowSystem::get().pollAndDispatchEvents();

    if (d->window)
    {
        d->window->glActivate();
    }
    else
    {
        GLWindow::glActivateMain();
    }

    Loop::nextLoopIteration();

    if (d->window)
    {
        d->window->glDone();
    }
}

} // namespace de
