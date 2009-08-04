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

#include "sdlopenglvideo.h"
#include "glwindow.h"

#include <de/App>
#include <SDL.h>

using namespace de;

SDLOpenGLVideo::SDLOpenGLVideo() : config_("/config/sdlopengl.de")
{
    std::cout << "SDLOpenGLVideo\n";
    config_.read();
    
    if(SDL_InitSubSystem(SDL_INIT_VIDEO))
    {
        throw SDLError("SDLOpenGLVideo::SDLOpenGLVideo", SDL_GetError());
    }
    
    Window::Placement place;
    Window::Mode mode;

    Config& cfg = App::config();
    place.topLeft = Vector2ui(cfg.getui("window.x"), cfg.getui("window.y"));
    place.setSize(Vector2ui(cfg.getui("window.width"), cfg.getui("window.height")));

    if(cfg.get("window.fullscreen").isTrue())
    {
        mode.set(Window::FULLSCREEN_BIT);
    }

    // Create the main window. 
    setMainWindow(newWindow(place, mode));
}

SDLOpenGLVideo::~SDLOpenGLVideo()
{
    std::cout << "~SDLOpenGLVideo\n";

    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void SDLOpenGLVideo::update(const Time::Delta& /*elapsed*/)
{
    SDL_PumpEvents();

    // Render the graphics in the main window.
    mainWindow().draw();
}

Window* SDLOpenGLVideo::newWindow(const Window::Placement& where, const Window::Mode& mode)
{
    // There can be only one.
    if(windows().size())
    {
        throw UnsupportedError("SDLOpenGLVideo::newWindow", "SDLOpenGLVideo can have only one window");
    }
    
    Window* w = new GLWindow(*this, where, mode);
    
    // Take ownership.
    windows().insert(w);
    
    return w;
}

void SDLOpenGLVideo::setTarget(de::Surface& surface)
{
    Video::setTarget(surface);
}

void SDLOpenGLVideo::releaseTarget()
{
    Video::releaseTarget();
    SDL_GL_SwapBuffers();
}
