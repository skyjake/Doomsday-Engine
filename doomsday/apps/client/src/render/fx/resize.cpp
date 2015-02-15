/** @file fx/resize.cpp  Change the size (pixel density) of the view.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "render/fx/resize.h"
#include "ui/clientwindow.h"
#include "clientapp.h"

#include <de/VRConfig>
#include <de/Drawable>
#include <de/GLFramebuffer>
#include <de/Variable>

#include <QList>

using namespace de;

namespace fx {

static Ranged const FACTOR_RANGE(1.0 / 16.0, 1.0);

DENG2_PIMPL(Resize)
{
    mutable Variable const *pixelDensity = nullptr;
    mutable Variable const *resizeFactor = nullptr;

    GLFramebuffer framebuf;
    Drawable frame;
    GLUniform uMvpMatrix { "uMvpMatrix", GLUniform::Mat4 };
    GLUniform uFrame     { "uTex",       GLUniform::Sampler2D };

    typedef GLBufferT<Vertex2Tex> VBuf;

    Instance(Public *i) : Base(i) {}

    GuiRootWidget &root() const
    {
        return ClientWindow::main().game().root();
    }

    void getConfig() const
    {
        if(!pixelDensity)
        {
            // Config variables.
            pixelDensity = &App::config("render.pixelDensity");
            resizeFactor = &App::config("render.fx.resize.factor");  
        }
    }

    float factor() const
    {
        getConfig();
    
        double const rf = (*resizeFactor > 0? 1.0 / *resizeFactor : 1.0);
        return FACTOR_RANGE.clamp(*pixelDensity * rf);
    }

    /// Determines if the post-processing shader will be applied.
    bool isActive() const
    {
        // This kind of scaling is not compatible with Oculus Rift -- LibOVR does its
        // own pixel density scaling.
        if(ClientApp::vr().mode() == VRConfig::OculusRift) return false;

        return !fequal(factor(), 1.f);
    }
    
    void glInit()
    {    
        framebuf.glInit();

        uFrame = framebuf.colorTexture();

        // Drawable for drawing stuff back to the original target.
        VBuf *buf = new VBuf;
        buf->setVertices(gl::TriangleStrip,
                         VBuf::Builder().makeQuad(Rectanglef(0, 0, 1, 1),
                                                  Rectanglef(0, 1, 1, -1)),
                         gl::Static);
        frame.addBuffer(buf);

        ClientApp::shaders().build(frame.program(), "generic.texture")
            << uMvpMatrix << uFrame;
    }

    void glDeinit()
    {
        LOGDEV_GL_XVERBOSE("Releasing GL resources");
        framebuf.glDeinit();
        frame.clear();
    }

    void update()
    {
        framebuf.resize(GLState::current().target().rectInUse().size() * factor());
        framebuf.setSampleCount(GLFramebuffer::defaultMultisampling());
    }

    void begin()
    {
        if(!isActive()) return;

        update();

        GLState::push()
                .setTarget(framebuf.target())
                .setViewport(Rectangleui::fromSize(framebuf.size()))
                .setColorMask(gl::WriteAll)
                .apply();
        framebuf.target().clear(GLTarget::ColorDepthStencil);
    }

    void end()
    {
        if(!isActive()) return;

        GLState::pop().apply();
    }

    void draw()
    {
        if(!isActive()) return;

        glEnable(GL_TEXTURE_2D);
        glDisable(GL_ALPHA_TEST);

        Rectanglef const vp = GLState::current().viewport();
        Vector2f targetSize = GLState::current().target().size();

        uMvpMatrix = Matrix4f::ortho(vp.left()   / targetSize.x,
                                     vp.right()  / targetSize.x,
                                     vp.top()    / targetSize.y,
                                     vp.bottom() / targetSize.y);

        GLState::push()
                .setBlend(false)
                .setDepthTest(false)
                .apply();

        frame.draw();

        GLState::pop().apply();

        glEnable(GL_ALPHA_TEST);
        glDisable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
    }
};

Resize::Resize(int console)
    : ConsoleEffect(console), d(new Instance(this))
{}

bool Resize::isActive() const
{
    return d->isActive();
}

void Resize::glInit()
{
    if(!d->isActive()) return;

    LOG_AS("fx::Resize");

    ConsoleEffect::glInit();
    d->glInit();
}

void Resize::glDeinit()
{
    LOG_AS("fx::Resize");

    d->glDeinit();
    ConsoleEffect::glDeinit();
}

void Resize::beginFrame()
{
    d->begin();
}

void Resize::endFrame()
{
    LOG_AS("fx::Resize");

    d->end();
    d->draw();

    if(!d->isActive() && isInited())
    {
        glDeinit();
    }
}

} // namespace fx
