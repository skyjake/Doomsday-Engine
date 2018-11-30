/** @file windowsystem.cpp  Window management subsystem.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include <de/BaseGuiApp> // for updating pixel ratio
#include <de/Map>

namespace de {

DE_PIMPL(WindowSystem)
, DE_OBSERVES(GLWindow, PixelRatio)
{
    typedef Map<String, BaseWindow *> Windows;
    Windows windows;

    std::unique_ptr<Style> style;

    // Mouse motion.
    bool mouseMoved;
    Vec2i latestMousePos;

    Impl(Public *i)
        : Base(i)
        , mouseMoved(false)
    {
        // Create a blank style by default.
        setStyle(new Style);
    }

    ~Impl()
    {
        self().closeAll();
    }

    void setStyle(Style *s)
    {
        style.reset(s);
        Style::setAppStyle(*s); // use as global style
    }

    void processLatestMousePosition()
    {
        self().rootProcessEvent(MouseEvent(MouseEvent::Absolute, latestMousePos));
    }

    void processLatestMousePositionIfMoved()
    {
        if (mouseMoved)
        {
            mouseMoved = false;
            processLatestMousePosition();
        }
    }

    void windowPixelRatioChanged(GLWindow &win)
    {
        if (&win == &BaseWindow::main())
        {
            DE_BASE_GUI_APP->setPixelRatio(float(win.pixelRatio()));
        }
    }
};

WindowSystem::WindowSystem()
    : System(ObservesTime | ReceivesInputEvents)
    , d(new Impl(this))
{}

void WindowSystem::setStyle(Style *style)
{
    d->setStyle(style);
}

void WindowSystem::addWindow(String const &id, BaseWindow *window)
{
    window->audienceForPixelRatio() += d;
    d->windows.insert(id, window);
}

bool WindowSystem::mainExists() // static
{
    return get().d->windows.contains(DE_STR("main"));
}

BaseWindow &WindowSystem::main() // static
{
    DE_ASSERT(mainExists());
    return *get().d->windows.find(DE_STR("main"))->second;
}

BaseWindow *WindowSystem::find(String const &id) const
{
    Impl::Windows::const_iterator found = d->windows.find(id);
    if (found != d->windows.end())
    {
        return found->second;
    }
    return nullptr;
}

LoopResult WindowSystem::forAll(const std::function<LoopResult (BaseWindow *)>& func)
{
    for (auto &win : d->windows)
    {
        if (auto result = func(win.second))
        {
            return result;
        }
    }
    return LoopContinue;
}

void WindowSystem::closeAll()
{
    closingAllWindows();

    d->windows.deleteAll();
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

Vec2i WindowSystem::latestMousePosition() const
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

    if (event.type() == Event::MousePosition)
    {
        MouseEvent const &mouse = event.as<MouseEvent>();

        if (mouse.pos() != d->latestMousePos)
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

static WindowSystem *theAppWindowSystem = nullptr;

void WindowSystem::setAppWindowSystem(WindowSystem &winSys)
{
    theAppWindowSystem = &winSys;
}

WindowSystem &WindowSystem::get() // static
{
    DE_ASSERT(theAppWindowSystem != nullptr);
    return *theAppWindowSystem;
}

} // namespace de
