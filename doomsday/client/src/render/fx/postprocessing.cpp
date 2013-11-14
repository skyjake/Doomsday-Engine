/** @file postprocessing.cpp World frame post processing.
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

#include "render/fx/postprocessing.h"
#include "ui/clientwindow.h"

#include <de/Drawable>
#include <de/GLTarget>

using namespace de;

namespace fx {

DENG2_PIMPL(PostProcessing)
{
    GLTexture texture;
    QScopedPointer<GLTarget> target;
    Drawable frame;
    GLUniform uMvpMatrix;
    GLUniform uFrame;

    typedef GLBufferT<Vertex2Tex> VBuf;

    Instance(Public *i)
        : Base(i)
        , uMvpMatrix("uMvpMatrix", GLUniform::Mat4)
        , uFrame    ("uTex",       GLUniform::Sampler2D)
    {}

    Canvas &canvas()
    {
        return ClientWindow::main().canvas();
    }

    GuiRootWidget &root()
    {
        return ClientWindow::main().game().root();
    }

    void glInit()
    {
        uMvpMatrix = Matrix4f::ortho(0, 1, 0, 1);
        uFrame = texture;

        texture.setFilter(gl::Nearest, gl::Nearest, gl::MipNone);
        texture.setUndefinedImage(canvas().size(), Image::RGBA_8888);
        target.reset(new GLTarget(texture, GLTarget::DepthStencil));

        // Drawable for drawing stuff back to the original target.
        VBuf *buf = new VBuf;
        buf->setVertices(gl::TriangleStrip,
                         VBuf::Builder().makeQuad(Rectanglef(0, 0, 1, 1), Rectanglef(0, 0, 1, 1)),
                         gl::Static);
        frame.addBuffer(buf);
        root().shaders().build(frame.program(), "generic.texture")
                << uMvpMatrix << uFrame;
    }

    void glDeinit()
    {
        texture.clear();
        target.reset();
    }

    void update()
    {
        if(!target.isNull())
        {
            target->resize(canvas().size());
        }
    }

    void begin()
    {
        update();
        target->clear(GLTarget::ColorDepthStencil);
        GLState::push().setTarget(*target).apply();
    }

    void end()
    {
        GLState::pop().apply();
    }

    void draw()
    {
        glEnable(GL_TEXTURE_2D);
        glDisable(GL_ALPHA_TEST);

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

PostProcessing::PostProcessing() : d(new Instance(this))
{}

void PostProcessing::glInit()
{
    d->glInit();
}

void PostProcessing::glDeinit()
{
    d->glDeinit();
}

void PostProcessing::begin()
{
    d->begin();
}

void PostProcessing::end()
{
    d->end();
}

void PostProcessing::drawResult()
{
    d->draw();
}

} // namespace fx
