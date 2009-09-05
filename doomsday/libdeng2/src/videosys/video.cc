/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Video"

using namespace de;

Video::Video() : _mainWindow(0), _target(0)
{}

Video::~Video()
{
    // Delete all windows.
    for(Windows::iterator i = _windows.begin(); i != _windows.end(); ++i)
    {
        delete *i;
    }
}

Window& Video::mainWindow() const
{
    assert(_mainWindow != 0);
    return *_mainWindow;
}
    
void Video::setMainWindow(Window* window)
{
    assert(window != 0);
    assert(_windows.find(window) != _windows.end());
    _mainWindow = window;
}

Surface* Video::target() const
{
    return _target;
}

void Video::setTarget(Surface& surface)
{
    _target = &surface;
}

void Video::releaseTarget()
{
    _target = 0;
}
