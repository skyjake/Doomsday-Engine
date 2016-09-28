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
#include "ui/clientwindow.h"
#include "render/viewports.h"
#include "world/p_players.h"
#include "api_render.h"
#include "clientapp.h"

#include <de/Config>
#include <de/GLState>
#include <de/GLShaderBank>
#include <de/Drawable>

using namespace de;

static Ranged const FACTOR_RANGE(1.0 / 16.0, 1.0);

DENG2_PIMPL(ViewCompositor)
{
    mutable Variable const *pixelDensity = nullptr;
    mutable Variable const *resizeFactor = nullptr;

    int playerNum = 0;

    /// Game view framebuffer. The latest game view is kept around for accessing at
    /// any time. This does not include additional layers such as the view border and
    /// game HUD.
    GLTextureFramebuffer viewFramebuf;

    Drawable frameDrawable;
    GLUniform uMvpMatrix { "uMvpMatrix", GLUniform::Mat4 };
    GLUniform uFrameTex  { "uTex",       GLUniform::Sampler2D };

    Impl(Public *i)
        : Base(i)
        , viewFramebuf(Image::RGBA_8888)
    {}

    ~Impl()
    {
        DENG2_ASSERT(!frameDrawable.isReady()); // deinited earlier
    }

    void getConfig() const
    {
        if (!pixelDensity)
        {
            // Config variables.
            pixelDensity = &App::config("render.pixelDensity");
            resizeFactor = &App::config("render.fx.resize.factor");
        }
    }

    double scaleFactor() const
    {
        getConfig();

        double const rf = (*resizeFactor > 0? 1.0 / *resizeFactor : 1.0);
        return FACTOR_RANGE.clamp(*pixelDensity * rf);
    }

    GLFramebuffer::Size framebufferSize() const
    {
        GLFramebuffer::Size size = R_Console3DViewRect(playerNum).size();

        // Pixel density.
        size *= scaleFactor();

        return size;
    }

    void glInit()
    {
        viewFramebuf.resize(framebufferSize());
        viewFramebuf.glInit();

        if (!frameDrawable.isReady())
        {
            ClientApp::shaders().build(frameDrawable.program(), "generic.texture")
                    << uMvpMatrix
                    << uFrameTex;

            using VBuf = GuiWidget::DefaultVertexBuf;

            auto *vbuf = new VBuf;
            frameDrawable.addBuffer(vbuf);

            VBuf::Builder verts;
            verts.makeQuad(Rectanglef(0, 0, 1, 1), Rectanglef(0, 1, 1, -1));
            vbuf->setVertices(gl::TriangleStrip, verts, gl::Static);
        }
    }

    void glDeinit()
    {
        viewFramebuf.glDeinit();
        frameDrawable.clear();
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
    d->glDeinit();
}

void ViewCompositor::renderGameView(std::function<void (int)> renderFunc)
{
    d->glInit();

    GLState::push()
            .setTarget(d->viewFramebuf)
            .setViewport(Rectangleui::fromSize(d->viewFramebuf.size()))
            .apply();

    d->viewFramebuf.clear(GLFramebuffer::ColorDepthStencil);

    // Rendering is done by the caller-provided callback.
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

void ViewCompositor::drawCompositedLayers(Rectanglei const &rect)
{
    DENG2_ASSERT(d->frameDrawable.isReady());

    GLState::push()
            .setAlphaTest(false)
            .setBlend    (false)
            .setDepthTest(false)
            .setCull     (gl::None);

    Rectanglei const view3D = R_Console3DViewRect(d->playerNum);

    // First the game view (using the previously rendered texture).
    d->uFrameTex  = d->viewFramebuf.colorTexture();
    d->uMvpMatrix = ClientWindow::main().root().projMatrix2D() *
                    Matrix4f::scaleThenTranslate(view3D.size(), view3D.topLeft);
    d->frameDrawable.draw();

    // View border around the game view.
    auto const oldDisplayPlayer = displayPlayer;
    displayPlayer = d->playerNum;

    R_UseViewPort(d->playerNum);

    //R_RenderPlayerViewBorder();

    // Game HUD.

    // Finale.

    // Non-map game screens.

    // Legacy engine/debug UIs (stuff from the old Net_Drawer).

    R_UseViewPort(nullptr);
    displayPlayer = oldDisplayPlayer;

    GLState::pop().apply();
}
