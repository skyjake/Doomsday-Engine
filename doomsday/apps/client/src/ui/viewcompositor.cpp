/** @file viewcompositor.cpp  Game view compositor.
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "ui/viewcompositor.h"
#include "api_render.h"

#include <de/GLState>

using namespace de;

DENG2_PIMPL(ViewCompositor)
{
    int playerNum = 0;

    /// Game view framebuffer. The latest game view is kept around for accessing at
    /// any time. This does not include additional layers such as the view border and
    /// game HUD.
    GLTextureFramebuffer viewFramebuf;

    Impl(Public *i)
        : Base(i)
        , viewFramebuf(Image::RGBA_8888)
    {}

    GLFramebuffer::Size framebufferSize() const
    {
        // TODO: Factor in pixel density and scaled-down window size.

        RectRaw geom;
        R_ViewPortGeometry(playerNum, &geom);
        return GLFramebuffer::Size(geom.size.width, geom.size.height);
    }

    void glInit()
    {
        viewFramebuf.resize(framebufferSize());
        viewFramebuf.glInit();
    }
};

ViewCompositor::ViewCompositor()
    : d(new Impl(this))
{}

void ViewCompositor::setPlayerNumber(int playerNum)
{
    d->playerNum = playerNum;
}

void ViewCompositor::glDeinit()
{
    d->viewFramebuf.glDeinit();
}

void ViewCompositor::renderGameView(std::function<void (int)> renderFunc)
{
    d->glInit();

    GLState::push()
            .setTarget(d->viewFramebuf)
            .setViewport(Rectangleui::fromSize(d->viewFramebuf.size()))
            .apply();

    d->viewFramebuf.clear(GLFramebuffer::ColorDepth);

    renderFunc(d->playerNum);

    GLState::pop()
            .apply();
}

GLTextureFramebuffer &ViewCompositor::gameView()
{
    return d->viewFramebuf;
}

GLTextureFramebuffer const &ViewCompositor::gameView() const
{
    return d->viewFramebuf;
}

void ViewCompositor::drawCompositedLayers()
{

}
