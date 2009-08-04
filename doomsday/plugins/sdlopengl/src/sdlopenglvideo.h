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

#include <de/Video>
#include <de/Surface>
#include <de/Config>

/**
 * Video subsystem implemented using SDL and OpenGL.
 */
class SDLOpenGLVideo : public de::Video
{
public:
    /// An operation that is not supported was attempted. @ingroup errors
    DEFINE_ERROR(UnsupportedError);
    
    /// SDL reported an error. @ingroup errors
    DEFINE_ERROR(SDLError);
    
public:    
    SDLOpenGLVideo();
    ~SDLOpenGLVideo();

    de::Window* newWindow(const de::Window::Placement& where, const de::Window::Mode& mode);
    void setTarget(de::Surface& surface);
    void releaseTarget();

    /**
     * Updates the contents of all windows.
     */
    void update(const de::Time::Delta& elapsed);

private:
    de::Config config_;
};
