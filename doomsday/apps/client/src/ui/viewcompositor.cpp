/** @file viewcompositor.cpp  Game view compositor.
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "ui/postprocessing.h"
#include "ui/infine/finaleinterpreter.h"
#include "ui/infine/finalepagewidget.h"
//#include "ui/editors/edit_bias.h"
#include "network/net_main.h" // Net_Drawer (get rid of this)
#include "render/rend_main.h"
#include "render/viewports.h"
#include "world/p_players.h"
#include "world/map.h"
#include "api_console.h"
#include "api_render.h"
#include "clientapp.h"
#include "dd_main.h"

#include <de/Config>
#include <de/CommandLine>
#include <de/GLState>
#include <de/GLShaderBank>
#include <de/Drawable>
#include <de/VRConfig>
#include <de/WindowTransform>

using namespace de;

static Ranged const FACTOR_RANGE(1.0 / 16.0, 1.0);

DENG2_PIMPL(ViewCompositor)
, DENG2_OBSERVES(Variable, Change)
{
    mutable Variable const *pixelDensity = nullptr;
    mutable Variable const *resizeFactor = nullptr;

    int playerNum = 0;

    /// Game view framebuffer. The latest game view is kept around for accessing at
    /// any time. This does not include additional layers such as the view border and
    /// game HUD.
    GLTextureFramebuffer viewFramebuf;

    PostProcessing postProcessing;
    //Drawable frameDrawable;
    //GLUniform uMvpMatrix { "uMvpMatrix", GLUniform::Mat4 };
    //GLUniform uFrameTex  { "uTex",       GLUniform::Sampler2D };

    Impl(Public *i)
        : Base(i)
        , viewFramebuf(Image::RGBA_8888)
    {}

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

        // Stereo splits warrant halving the resolution because each eye uses up half of
        // the window.
        switch (ClientApp::vr().mode())
        {
        case VRConfig::SideBySide:
            size.x /= 2;
            break;

        case VRConfig::TopBottom:
            size.y /= 2;
            break;

        default:
            break;
        }

        // Pixel density.
        size *= scaleFactor();

        return size;
    }

    void updateSampleCount()
    {
        int sampleCount = 1;

        bool configured = App::config().getb("window.main.fsaa");
        if (App::commandLine().has("-nofsaa") || !configured)
        {
            LOG_GL_VERBOSE("Multisampling off");
        }
        else
        {
            sampleCount = 4; // four samples is fine?
            LOG_GL_VERBOSE("Multisampling on (%i samples)") << sampleCount;
        }

        viewFramebuf.setSampleCount(sampleCount);
    }

    void variableValueChanged(Variable &, Value const &) override
    {
        updateSampleCount();
    }

    void glInit()
    {
        viewFramebuf.resize(framebufferSize());
        if (!viewFramebuf.areTexturesReady())
        {
            App::config("window.main.fsaa").audienceForChange() += this;
            updateSampleCount();

            viewFramebuf.glInit();
        }

        postProcessing.glInit();


        /*if (!frameDrawable.isReady())
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
        }*/
    }

    void glDeinit()
    {
        viewFramebuf.glDeinit();
        postProcessing.glDeinit();
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

void ViewCompositor::drawCompositedLayers()
{
    Rectanglei const view3D = R_Console3DViewRect(d->playerNum);
    auto const oldDisplayPlayer = displayPlayer;

    // View border around the game view.
    displayPlayer = d->playerNum;

    R_UseViewPort(d->playerNum);

    GLState::push()
            .setAlphaTest(false)
            .setBlend    (false)
            .setDepthTest(false)
            .setCull     (gl::None);

    // 3D world view (using the previously rendered texture).
    d->postProcessing.update();
    d->postProcessing.draw(ClientWindow::main().root().projMatrix2D() *
                           Matrix4f::scaleThenTranslate(view3D.size(), view3D.topLeft),
                           d->viewFramebuf.colorTexture());

    // Some of the layers use OpenGL 2 drawing code.
    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PushMatrix();
    DGL_LoadMatrix(ClientWindow::main().root().projMatrix2D().values());

    // Fill around a scaled-down 3D view. The border is not visible if the 3D view
    // covers the entire area.

    // Game HUD.
    if (auto const *vp = R_CurrentViewPort())
    {
        R_RenderPlayerViewBorder();

        /// @todo HUD rendering probably doesn't need the vdWindow (maybe for the automap?).

        RectRaw vpGeometry(vp->geometry.topLeft.x, vp->geometry.topLeft.y,
                           vp->geometry.width(), vp->geometry.height());

        viewdata_t const *vd = &DD_Player(d->playerNum)->viewport();
        RectRaw vdWindow(vd->window.topLeft.x, vd->window.topLeft.y,
                         vd->window.width(), vd->window.height());

        if (gx.DrawViewPort)
        {
            GLState::current()
                    .setBlend(true)
                    .apply();

            gx.DrawViewPort(P_ConsoleToLocal(d->playerNum),
                            &vpGeometry,
                            &vdWindow,
                            displayPlayer,
                            /* layer: */ 1);
        }
    }

    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PopMatrix();

    // The console rectangles are defined in logical UI coordinates. We need to map to
    // normalized window coordinates first because the rendering target may not be
    // the window (the target may thus have any resolution).

    Rectanglef const normRect = GuiWidget::normalizedRect(
                R_ConsoleRect(d->playerNum),
                Rectanglei::fromSize(ClientWindow::main().root().viewSize()));

    auto const targetSize = GLState::current().target().size();
    Rectanglef const vp { normRect.topLeft     * targetSize,
                          normRect.bottomRight * targetSize };

    GLState::push()
            .setViewport(vp.toRectangleui())
            .apply();

    // Finale.
    {
        if (App_InFineSystem().finaleInProgess())
        {
            dgl_borderedprojectionstate_t bp;
            GL_ConfigureBorderedProjection(&bp, BPF_OVERDRAW_CLIP |
                                           (!App_World().hasMap()? BPF_OVERDRAW_MASK : 0),
                                           SCREENWIDTH, SCREENHEIGHT,
                                           DENG_GAMEVIEW_WIDTH, DENG_GAMEVIEW_HEIGHT,
                                           scalemode_t(Con_GetByte("rend-finale-stretch")));
            GL_BeginBorderedProjection(&bp);
            for (Finale *finale : App_InFineSystem().finales())
            {
                finale->interpreter().page(FinaleInterpreter::Anims).draw();
                finale->interpreter().page(FinaleInterpreter::Texts).draw();
            }
            GL_EndBorderedProjection(&bp);
        }
    }

    // Non-map game screens.
    {
        // Draw any full window game graphics.
        if (gx.DrawWindow)
        {
            Size2Raw const dimensions(DENG_GAMEVIEW_WIDTH, DENG_GAMEVIEW_HEIGHT);
            gx.DrawWindow(&dimensions);
        }
    }

    GLState::pop().apply();

    // Legacy engine/debug UIs (stuff from the old Net_Drawer).
    {
#if 0
        // Draw the widgets of the Shadow Bias Editor (if active).
        SBE_DrawGui();

        // Debug visualizations.
        if (App_World().hasMap() && App_World().map().hasLightGrid())
        {
            Rend_LightGridVisual(App_World().map().lightGrid());
        }
#endif
        Net_Drawer();
        Sfx_ChannelDrawer();
    }

    // Restore the default drawing state.
    R_UseViewPort(nullptr);
    displayPlayer = oldDisplayPlayer;

    GLState::considerNativeStateUndefined();
    GLState::pop().apply();
}

PostProcessing &ViewCompositor::postProcessing()
{
    return d->postProcessing;
}
