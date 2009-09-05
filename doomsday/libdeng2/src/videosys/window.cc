/*
 * The Doomsday Engine Project -- libdeng2
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

#include "de/Window"
#include "de/Surface"
#include "de/Video"

using namespace de;

Window::Window(Video& v, const Placement& p, const Mode& m, Surface* surf) 
    : _video(v), _place(p), _mode(m), _surface(surf)
{}

Window::~Window()
{
    delete _surface;
}

void Window::setPlace(const Placement& p)
{
    _place = p;
    
    // Update surface size.
    _surface->setSize(p.size());
}

Surface& Window::surface() const
{
    assert(_surface != 0);
    return *_surface;
}

void Window::setSurface(Surface* surf)
{
    _surface = surf;
}

void Window::setMode(Flag modeFlags, bool set)
{
    Mode flags(modeFlags);
    if(set)
    {
        _mode = _mode | flags;
    }
    else
    {
        _mode = _mode & ~flags;
    }
}

void Window::draw()
{
    // Use this window as the rendering target.
    _video.setTarget(surface());

    // Draw all the visuals.
    _root.draw();
    
    _video.releaseTarget();
}
