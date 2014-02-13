/** @file basewindow.cpp  Abstract base class for application windows.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/BaseWindow"
#include "de/WindowTransform"
#include "de/WindowSystem"

#include <de/GuiApp>

namespace de {

DENG2_PIMPL(BaseWindow)
, DENG2_OBSERVES(KeyEventSource,   KeyEvent)
, DENG2_OBSERVES(MouseEventSource, MouseEvent)
{
    WindowTransform defaultXf; ///< Used by default (doesn't apply any transformation).
    WindowTransform *xf;

    Instance(Public *i)
        : Base(i)
        , defaultXf(self)
        , xf(&defaultXf)
    {
        // Listen to input.
        self.canvas().audienceForKeyEvent   += this;
        self.canvas().audienceForMouseEvent += this;
    }

    ~Instance()
    {
        self.canvas().audienceForKeyEvent   -= this;
        self.canvas().audienceForMouseEvent -= this;
    }

    void keyEvent(KeyEvent const &ev)
    {
        /// @todo Input drivers should observe the notification instead, input
        /// subsystem passes it to window system. -jk

        // Pass the event onto the window system.
        if(!WindowSystem::appWindowSystem().processEvent(ev))
        {
            // Maybe the fallback handler has use for this.
            self.handleFallbackEvent(ev);
        }
    }

    void mouseEvent(MouseEvent const &event)
    {
        MouseEvent ev = event;

        // Translate mouse coordinates for direct interaction.
        if(ev.type() == Event::MousePosition ||
           ev.type() == Event::MouseButton ||
           ev.type() == Event::MouseWheel)
        {
            ev.setPos(xf->windowToLogicalCoords(event.pos()).toVector2i());
        }

        if(!WindowSystem::appWindowSystem().processEvent(ev))
        {
            // Maybe the fallback handler has use for this.
            self.handleFallbackEvent(ev);
        }
    }
};

BaseWindow::BaseWindow(String const &id)
    : PersistentCanvasWindow(id)
    , d(new Instance(this))
{}

void BaseWindow::setTransform(WindowTransform &xf)
{
    d->xf = &xf;
}

void BaseWindow::useDefaultTransform()
{
    d->xf = &d->defaultXf;
}

WindowTransform &BaseWindow::transform()
{
    DENG2_ASSERT(d->xf != 0);
    return *d->xf;
}

bool BaseWindow::shouldRepaintManually() const
{
    // By default always prefer updates that are "nice" to the rest of the system.
    return false;
}

bool BaseWindow::prepareForDraw()
{
    // Don't run the main loop until after the paint event has been dealt with.
    DENG2_GUI_APP->loop().pause();
    return true; // Go ahead.
}

void BaseWindow::draw()
{
    if(!prepareForDraw())
    {
        // Not right now, please.
        return;
    }

    if(shouldRepaintManually())
    {
        DENG2_ASSERT_IN_MAIN_THREAD();

        // Perform the drawing manually right away.
        canvas().makeCurrent();
        canvas().updateGL();
    }
    else
    {
        // Request update at the earliest convenience.
        canvas().update();
    }
}

void BaseWindow::canvasGLDraw(Canvas &cv)
{
    preDraw();
    d->xf->drawTransformed();
    postDraw();

    PersistentCanvasWindow::canvasGLDraw(cv);
}

void BaseWindow::preDraw()
{}

void BaseWindow::postDraw()
{}

} // namespace de
