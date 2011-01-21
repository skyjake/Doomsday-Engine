/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2009, 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "video.h"
#include "surface.h"
#include "clientapp.h"

using namespace de;

Video::Video() : _mainWindow(0), _target(0)
{}

Video::~Video()
{
    // Delete all windows.
    foreach(Window* win, _windows)
    {
        delete win;
    }
}

Window& Video::mainWindow() const
{
    Q_ASSERT(_mainWindow != 0);
    return *_mainWindow;
}
    
void Video::addWindow(Window* window)
{
    Q_ASSERT(window != 0);
    Q_ASSERT(!_windows.contains(window));
    _windows.insert(window);

    connect(window, SIGNAL(destroyed(QObject*)), this, SLOT(windowDestroyed(QObject*)));
}

void Video::windowDestroyed(QObject* window)
{
    removeWindow(static_cast<Window*>(window));
}

void Video::removeWindow(Window* window)
{
    Q_ASSERT(window != 0);
    Q_ASSERT(_windows.contains(window));

    _windows.remove(window);
}

void Video::setMainWindow(Window* window)
{
    Q_ASSERT(window != 0);
    Q_ASSERT(_windows.find(window) != _windows.end());
    _mainWindow = window;
}

Surface* Video::target() const
{
    return _target;
}

void Video::setTarget(Surface& surface)
{
    // TODO: Stack needed?

    Q_ASSERT(_target == 0);
    _target = &surface;
}

void Video::releaseTarget(Surface& surface)
{
    Q_ASSERT(_target == &surface);
    _target = 0;
}

void Video::update(const Time::Delta& /*elapsed*/)
{}

Video& theVideo()
{
    return theApp().video();
}
