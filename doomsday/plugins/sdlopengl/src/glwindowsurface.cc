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

#include "glwindowsurface.h"

#include <SDL.h>

using namespace de;

GLWindowSurface::GLWindowSurface(const Size& size, GLWindow* owner)
    : de::Surface(size), owner_(owner)
{}

GLWindowSurface::~GLWindowSurface()
{}

duint GLWindowSurface::colorDepth() const
{
    SDL_Surface* surf = SDL_GetVideoSurface();
    assert(surf != 0);
    return surf->format->BitsPerPixel;
}
