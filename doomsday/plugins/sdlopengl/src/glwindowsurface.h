/*
 * The Doomsday Engine Project
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

#ifndef GLWINDOWSURFACE_H
#define GLWINDOWSURFACE_H

#include <de/Surface>

class GLWindow;

/**
 * GLWindowSurface allows drawing into a window.
 */
class GLWindowSurface : public de::Surface
{
public:
    GLWindowSurface(const Size& size, GLWindow* owner);
    ~GLWindowSurface();

    de::duint colorDepth() const;
    
private:  
    /// The window where this surface is drawing to.
    GLWindow* owner_;
};

#endif /* GLWINDOWSURFACE_H */
