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
    : video_(v), place_(p), mode_(m), surface_(surf)
{}

Window::~Window()
{
    delete surface_;
}

void Window::setPlace(const Placement& p)
{
    place_ = p;
    
    // Update surface size.
    surface_->setSize(p.size());
}

Surface& Window::surface() const
{
    assert(surface_ != 0);
    return *surface_;
}

void Window::setSurface(Surface* surf)
{
    surface_ = surf;
}

void Window::setMode(Flag modeFlags, bool set)
{
    Mode flags(modeFlags);
    if(set)
    {
        mode_ = mode_ | flags;
    }
    else
    {
        mode_ = mode_ & ~flags;
    }
}

void Window::draw()
{
    // Use this window as the rendering target.
    video_.setTarget(surface());

    // Draw all the visuals.
    root_.draw();
    
    video_.releaseTarget();
}
