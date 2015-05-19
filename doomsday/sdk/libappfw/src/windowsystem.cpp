/** @file windowsystem.cpp  Window management subsystem.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/WindowSystem"
#include <de/Style>
#include <QMap>

namespace de {

DENG2_PIMPL(WindowSystem)
{
    typedef QMap<String, BaseWindow *> Windows;
    Windows windows;

    QScopedPointer<Style> style;

    // Mouse motion.
    bool mouseMoved;
    Vector2i latestMousePos;

    Instance(Public *i)
        : Base(i)
        , mouseMoved(false)
    {
        // Create a blank style by default.
        setStyle(new Style);
    }

    ~Instance()
    {
        self.closeAll();
    }

    void setStyle(Style *s)
    {
        style.reset(s);
        Style::setAppStyle(*s); // use as global style
    }

    void processLatestMousePosition()
    {
        self.rootProcessEvent(MouseEvent(MouseEvent::Absolute, latestMousePos));
    }

    void processLatestMousePositionIfMoved()
    {
        if(mouseMoved)
        {
            mouseMoved = false;
            processLatestMousePosition();
        }
    }
};

WindowSystem::WindowSystem()
    : System(ObservesTime | ReceivesInputEvents)
    , d(new Instance(this))
{}

void WindowSystem::setStyle(Style *style)
{
    d->setStyle(style);
}

void WindowSystem::addWindow(String const &id, BaseWindow *window)
{
    d->windows.insert(id, window);
}

bool WindowSystem::mainExists() // static
{
    return get().d->windows.contains("main");
}

BaseWindow &WindowSystem::main() // static
{
    DENG2_ASSERT(mainExists());
    return **get().d->windows.find("main");
}

BaseWindow *WindowSystem::find(String const &id) const
{
    Instance::Windows::const_iterator found = d->windows.constFind(id);
    if(found != d->windows.constEnd())
    {
        return found.value();
    }
    return 0;
}

void WindowSystem::closeAll()
{   
    closingAllWindows();

    qDeleteAll(d->windows.values());
    d->windows.clear();
}

Style &WindowSystem::style()
{
    return *d->style;
}

void WindowSystem::dispatchLatestMousePosition()
{
    d->processLatestMousePosition();
}

Vector2i WindowSystem::latestMousePosition() const
{
    return d->latestMousePos;
}

bool WindowSystem::processEvent(Event const &event)
{
    /*
     * Mouse motion is filtered as it may be produced needlessly often with
     * high-frequency mice. Note that this does not affect DirectInput mouse
     * input at all (however, as DirectInput is polled once per frame, it is
     * already filtered anyway).
     */

    if(event.type() == Event::MousePosition)
    {
        MouseEvent const &mouse = event.as<MouseEvent>();

        if(mouse.pos() != d->latestMousePos)
        {
            // This event will be emitted later, before widget tree update.
            d->latestMousePos = mouse.pos();
            d->mouseMoved = true;
        }
        return true;
    }

    // Dispatch the event to the main window's widget tree.
    return rootProcessEvent(event);
}

void WindowSystem::timeChanged(Clock const &/*clock*/)
{
    d->processLatestMousePositionIfMoved();

    // Update periodically.
    rootUpdate();
}

void WindowSystem::closingAllWindows()
{}

static WindowSystem *theAppWindowSystem = 0;

void WindowSystem::setAppWindowSystem(WindowSystem &winSys)
{
    theAppWindowSystem = &winSys;
}

WindowSystem &WindowSystem::get() // static
{
    DENG2_ASSERT(theAppWindowSystem != 0);
    return *theAppWindowSystem;
}

} // namespace de
