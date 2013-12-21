/** @file busywidget.cpp
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de_platform.h"
#include "ui/widgets/busywidget.h"
#include "ui/widgets/progresswidget.h"
#include "ui/busyvisual.h"
#include "ui/ui_main.h"
#include "ui/clientwindow.h"
#include "gl/gl_main.h"
#include "render/r_main.h"
#include "GuiRootWidget"
#include "busymode.h"
#include "sys_system.h"
#include "con_main.h"

#include <de/concurrency.h>
#include <de/Drawable>
#include <de/GLFramebuffer>

using namespace de;

DENG_GUI_PIMPL(BusyWidget)
{
    typedef DefaultVertexBuf VertexBuf;

    ProgressWidget *progress;
    Time frameDrawnAt;
    GLFramebuffer transitionFrame;
    Drawable drawable;
    GLUniform uTex;
    GLUniform uMvpMatrix;

    Instance(Public *i)
        : Base(i),
          uTex("uTex", GLUniform::Sampler2D),
          uMvpMatrix("uMvpMatrix", GLUniform::Mat4)
    {
        progress = new ProgressWidget;
        progress->setAlignment(ui::AlignCenter, LabelWidget::AlignOnlyByImage);
        progress->setRange(Rangei(0, 200));
        progress->setImageScale(.2f);
        progress->rule().setRect(self.rule());
        self.add(progress);
    }

    void glInit()
    {
        //transitionFrame.setColorFormat(Image::RGB_888);

        VertexBuf *buf = new VertexBuf;

        VertexBuf::Builder verts;
        verts.makeQuad(Rectanglef(0, 0, 1, 1), Vector4f(1, 1, 1, 1), Rectanglef(0, 0, 1, 1));
        buf->setVertices(gl::TriangleStrip, verts, gl::Static);

        drawable.addBuffer(buf);
        shaders().build(drawable.program(), "generic.textured.color")
                << uMvpMatrix << uTex;
    }

    void glDeinit()
    {
        drawable.clear();
        transitionFrame.glDeinit();
    }

    bool haveTransitionFrame() const
    {
        return transitionFrame.isReady();
    }
};

BusyWidget::BusyWidget(String const &name)
    : GuiWidget(name), d(new Instance(this))
{
    requestGeometry(false);
}

ProgressWidget &BusyWidget::progress()
{
    return *d->progress;
}

void BusyWidget::viewResized()
{
    GuiWidget::viewResized();
}

void BusyWidget::update()
{
    GuiWidget::update();

    if(BusyMode_Active())
    {
        BusyMode_Loop();
    }
}

void BusyWidget::drawContent()
{
    if(!BusyMode_Active())
    {
        d->progress->hide();

        if(Con_TransitionInProgress())
        {
            GLState::push()
                    .setViewport(Rectangleui::fromSize(GLState::current().target().size()))
                    .apply();
            Con_DrawTransition();
            GLState::pop().apply();
        }
        else
        {
            // Time to hide the busy widget, the transition has ended (or
            // was never started).
            hide();
            releaseTransitionFrame();
        }
        return;
    }

    if(d->haveTransitionFrame())
    {
        GLState::current().apply();

        glDisable(GL_ALPHA_TEST); /// @todo get rid of these
        glDisable(GL_BLEND);
        glEnable(GL_TEXTURE_2D);

        // Draw the texture.
        Rectanglei pos = rule().recti();
        d->uMvpMatrix = Matrix4f::scale(Vector3f(1, -1, 1)) *
                root().projMatrix2D() *
                Matrix4f::scaleThenTranslate(pos.size(), pos.topLeft);
        d->drawable.draw();

        glEnable(GL_ALPHA_TEST);
        glEnable(GL_BLEND);
        glDisable(GL_TEXTURE_2D);
    }
}

bool BusyWidget::handleEvent(Event const &)
{
    // Eat events and ignore them.
    return true;
}

static TimeDelta const TRANSITION_FRAME_VALID_DURATION = 0.5; // seconds

void BusyWidget::renderTransitionFrame()
{
    LOG_AS("BusyWidget");

    if(d->haveTransitionFrame() && d->frameDrawnAt.since() < TRANSITION_FRAME_VALID_DURATION)
    {
        // We already have a valid frame, no need to render again.
        LOG_DEBUG("Skipping rendering of transition frame (got one already)");
        return;
    }

    // We'll have an up-to-date frame after this.
    d->frameDrawnAt = Time();

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    //GLTexture::Size size(rule().width().valuei() / 2,
    //                         rule().height().valuei() / 2);

    Rectanglei grabRect = Rectanglei::fromSize(root().window().canvas().size());

    LOG_DEBUG("Rendering transition frame, size ") << grabRect.size().asText();

    /*
    if(BusyMode_IsTransitionAnimated())
    {
        // Animation transitions are drawn only inside GameWidget, so just
        // grab that portion of the screen.
        grabRect = root().window().game().rule().recti();
    }

    // Grab the game view's rectangle, as that's where the transition will be drawn.
    GLuint grabbed = root().window().grabAsTexture(grabRect, ClientWindow::GrabHalfSized);*/

    d->transitionFrame.resize(grabRect.size());
    if(!d->transitionFrame.isReady())
    {
        d->transitionFrame.glInit();
    }

    GLState::push()
            .setTarget(d->transitionFrame.target())
            .setViewport(Rectangleui::fromSize(d->transitionFrame.size()))
            .apply();

    //d->transitionTex.reset(new GLTexture); //grabbed, grabRect.size() / 2));
    //d->transitionTex->setUndefinedImage(grabRect.size(), Image::RGB_888);

    root().window().drawGameContent();

    GLState::pop().apply();

    d->uTex = d->transitionFrame.colorTexture();
}

void BusyWidget::releaseTransitionFrame()
{
    if(d->haveTransitionFrame())
    {
        LOG_DEBUG("Releasing transition frame");
        d->transitionFrame.glDeinit();
    }
}

GLTexture const *BusyWidget::transitionFrame() const
{
    if(d->haveTransitionFrame())
    {
        return &d->transitionFrame.colorTexture();
    }
    return 0;
}

void BusyWidget::glInit()
{
    d->glInit();
}

void BusyWidget::glDeinit()
{
    d->glDeinit();
}
