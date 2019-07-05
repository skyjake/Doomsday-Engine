/** @file appwindowsystem.cpp  Application window system.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "shellwindowsystem.h"
#include "linkwindow.h"
#include <de/App>
#include <de/PackageLoader>

using namespace de;

DE_PIMPL(ShellWindowSystem)
{
    String focused;

    Impl(Public *i) : Base(i)
    {
        self().style().load(App::packageLoader().package("net.dengine.stdlib.gui"));
    }
};

ShellWindowSystem::ShellWindowSystem() : d(new Impl(this))
{
    setAppWindowSystem(*this);
}

LinkWindow &ShellWindowSystem::main() // static
{
    return WindowSystem::main().as<LinkWindow>();
}

LinkWindow *ShellWindowSystem::focusedWindow() // static
{
    auto &sys = static_cast<ShellWindowSystem &>(WindowSystem::get());
    return maybeAs<LinkWindow>(sys.find(sys.d->focused));
}

void ShellWindowSystem::setFocusedWindow(const String &id)
{
    d->focused = id;
}

bool ShellWindowSystem::rootProcessEvent(const Event &event)
{
    if (auto *win = focusedWindow())
    {
        return win->root().processEvent(event);
    }
    return false;
}

void ShellWindowSystem::rootUpdate()
{
    forAll([](BaseWindow *win) {
        if (auto *linkWindow = maybeAs<LinkWindow>(win))
        {
            linkWindow->root().update();
        }
        return LoopContinue;
    });
}
