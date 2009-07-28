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

#include "glwindow.h"
#include "glwindowsurface.h"
#include "sdlopenglvideo.h"

#include <SDL.h>

using namespace de;

GLWindow::GLWindow(SDLOpenGLVideo& video, const Placement& place, const Mode& mode)
    : Window(video, place, mode)
{
    setSurface(new GLWindowSurface(place.size(), this));
    setSDLVideoMode();
}

SDLOpenGLVideo& GLWindow::sdlVideo() const
{
    return static_cast<SDLOpenGLVideo&>(video());
}

void GLWindow::setMode(Flag modeFlags, bool yes)
{
    //setSDLVideoMode();
}

void GLWindow::setTitle(const std::string& title)
{
    SDL_WM_SetCaption(title.c_str(), 0);
}

void GLWindow::setSDLVideoMode()
{
    // Configure OpenGL.
    /// @todo Consult App configuration.
    //SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    //SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);

    duint flags = SDL_OPENGL;
    if(mode()[FULLSCREEN_BIT])
    {
        flags |= SDL_FULLSCREEN;
    }
    else
    {
        flags |= SDL_RESIZABLE;
    }

    if(!SDL_SetVideoMode(place().width(), place().height(), 0, flags))
    {
        // Without multisample?
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);

        if(!SDL_SetVideoMode(place().width(), place().height(), 0, flags))
        {
            throw SDLOpenGLVideo::SDLError("GLWindow::setSDLVideoMode", SDL_GetError());
        }    
    }
}
