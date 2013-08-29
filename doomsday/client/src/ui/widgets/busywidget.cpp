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
#include "ui/framework/guirootwidget.h"
#include "ui/busyvisual.h"
#include "busymode.h"
#include "sys_system.h"
#include "render/r_main.h"
#include "ui/ui_main.h"
#include "ui/clientwindow.h"

#include <de/RootWidget>
#include <de/Drawable>
#include <de/GLTexture>

using namespace de;

DENG_GUI_PIMPL(BusyWidget)
{
    typedef DefaultVertexBuf VertexBuf;

    ProgressWidget *progress;
    QScopedPointer<GLTexture> transitionTex;
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
    }
};

BusyWidget::BusyWidget(String const &name)
    : GuiWidget(name), d(new Instance(this))
{}

ProgressWidget &BusyWidget::progress()
{
    return *d->progress;
}

void BusyWidget::viewResized()
{
    if(!BusyMode_Active() || isDisabled() || Sys_IsShuttingDown()) return;

    ClientWindow::main().glActivate(); // needed for legacy stuff

    //DENG_ASSERT(BusyMode_Active());

    LOG_AS("BusyWidget");
    LOG_DEBUG("View resized to ") << root().viewSize().asText();

    // Update viewports.
    R_SetViewGrid(0, 0);
    R_UseViewPort(0);
    R_LoadSystemFonts();

    if(UI_IsActive())
    {
        UI_UpdatePageLayout();
    }
}

void BusyWidget::update()
{
    GuiWidget::update();

    DENG_ASSERT(BusyMode_Active());
    BusyMode_Loop();
}

void BusyWidget::drawContent()
{
    if(d->transitionTex.isNull())
    {
        root().window().canvas().renderTarget().clear(GLTarget::ColorDepth);
    }
    else
    {
        // Draw the texture.
        Rectanglei pos = rule().recti();
        d->uMvpMatrix = root().projMatrix2D() *
                Matrix4f::scaleThenTranslate(pos.size(), pos.topLeft);
        d->drawable.draw();
    }
}

bool BusyWidget::handleEvent(Event const &)
{
    // Eat events and ignore them.
    return true;
}

void BusyWidget::grabTransitionScreenshot()
{
    GLTexture::Size size(rule().width().valuei() / 2,
                         rule().height().valuei() / 2);

    GLuint grabbed = root().window().grabAsTexture(ClientWindow::GrabHalfSized);

    d->transitionTex.reset(new GLTexture(grabbed, size));
    d->uTex = *d->transitionTex;
}

void BusyWidget::releaseTransitionScreenshot()
{
    d->transitionTex.reset();
}

GLTexture const *BusyWidget::transitionScreenshot() const
{
    return d->transitionTex.data();
}

void BusyWidget::glInit()
{
    d->glInit();
}

void BusyWidget::glDeinit()
{
    d->glDeinit();
}
