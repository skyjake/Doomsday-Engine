/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009, 2010 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "surface.h"
#include "video.h"

using namespace de;

Surface::~Surface()
{}

QImage Surface::captureImage() const
{
    throw CaptureError("Surface::captureImage", "Surface cannot be converted to image");
}

void Surface::activate()
{
    // Tell the video subsystem to use this surface as the rendering target.
    theVideo().setTarget(*this);
}

void Surface::deactivate()
{
    theVideo().releaseTarget(*this);
}
