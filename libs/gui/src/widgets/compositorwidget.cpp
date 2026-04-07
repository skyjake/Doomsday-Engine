/** @file compositorwidget.cpp  Off-screen compositor.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/compositorwidget.h"

#include <de/drawable.h>

namespace de {

DE_GUI_PIMPL(CompositorWidget)
{
    Drawable drawable;
    struct Buffer {
        GLTexture texture;
        std::unique_ptr<GLFramebuffer> offscreen;

        void clear() {
            texture.clear();
            offscreen.reset();
        }
    };
    int nextBufIndex;
    List<Buffer *> buffers; ///< Stack of buffers to allow nested compositing.
    GLUniform uMvpMatrix;
    GLUniform uTex;

    Impl(Public *i)
        : Base(i),
          nextBufIndex(0),
          uMvpMatrix("uMvpMatrix", GLUniform::Mat4),
          uTex      ("uTex",       GLUniform::Sampler2D)
    {
        uMvpMatrix = Mat4f::ortho(0, 1, 0, 1);
    }

    /**
     * Starts using the next unused buffer. The buffer is resized if needed.
     * The size of the new buffer is the same as the size of the current GL
     * target.
     */
    Buffer *beginBufferUse()
    {
        if (nextBufIndex <= buffers.sizei())
        {
            buffers.append(new Buffer);
        }

        Buffer *buf = buffers[nextBufIndex];
        const Vec2ui size = GLState::current().target().rectInUse().size();
        //qDebug() << "compositor" << nextBufIndex << "should be" << size.asText();
        //qDebug() << buf->texture.size().asText() << size.asText();
        if (buf->texture.size() != size)
        {
            //qDebug() << "buffer texture defined" << size.asText();
            buf->texture.setUndefinedImage(size, Image::RGBA_8888);
            buf->offscreen.reset(new GLFramebuffer(buf->texture));
        }
        nextBufIndex++;
        return buf;
    }

    void endBufferUse()
    {
        nextBufIndex--;
    }

    void glInit()
    {
        DefaultVertexBuf *buf = new DefaultVertexBuf;
        buf->setVertices(gfx::TriangleStrip,
                         DefaultVertexBuf::Builder()
                            .makeQuad(Rectanglef(0, 0, 1, 1),
                                      Vec4f  (1, 1, 1, 1),
                                      Rectanglef(0, 0, 1, -1)),
                         gfx::Static);

        drawable.addBuffer(buf);
        root().shaders().build(drawable.program(), "generic.textured.color") //"debug.textured.alpha"
                << uMvpMatrix
                << uTex;
    }

    void glDeinit()
    {
        deleteAll(buffers);
        buffers.clear();
        drawable.clear();
    }

    bool shouldBeDrawn() const
    {
        return self().isInitialized() && !self().isHidden() && self().visibleOpacity() > 0 &&
               GLState::current().target().rectInUse().size() != Vec2ui();
    }
};

CompositorWidget::CompositorWidget(const String &name)
    : GuiWidget(name), d(new Impl(this))
{}

GLTexture &CompositorWidget::composite() const
{
    DE_ASSERT(!d->buffers.isEmpty());
    return d->buffers.first()->texture;
}

void CompositorWidget::setCompositeProjection(const Mat4f &projMatrix)
{
    d->uMvpMatrix = projMatrix;
}

void CompositorWidget::useDefaultCompositeProjection()
{
    d->uMvpMatrix = Mat4f::ortho(0, 1, 0, 1);
}

void CompositorWidget::viewResized()
{
    GuiWidget::viewResized();
}

void CompositorWidget::preDrawChildren()
{
    GuiWidget::preDrawChildren();
    if (d->shouldBeDrawn())
    {
        root().painter().flush();

        //qDebug() << "entering compositor" << d->nextBufIndex;

        Impl::Buffer *buf = d->beginBufferUse();
        DE_ASSERT(buf->offscreen);

        GLState::push()
                .setTarget(*buf->offscreen)
                .setViewport(Rectangleui::fromSize(buf->texture.size()));

        buf->offscreen->clear(GLFramebuffer::Color0);
    }
}

void CompositorWidget::postDrawChildren()
{
    GuiWidget::postDrawChildren();
    if (d->shouldBeDrawn())
    {
        root().painter().flush();

        // Restore original rendering target.
        GLState::pop();

        drawComposite();

        d->endBufferUse();
    }
}

void CompositorWidget::glInit()
{
    d->glInit();
}

void CompositorWidget::glDeinit()
{
    d->glDeinit();
}

void CompositorWidget::drawComposite()
{
    if (!d->shouldBeDrawn()) return;

    DE_ASSERT(d->nextBufIndex > 0);

    Impl::Buffer *buf = d->buffers[d->nextBufIndex - 1];

    GLState::push()
            .setAlphaTest(false)
            .setBlend(true)
            .setBlendFunc(gfx::One, gfx::OneMinusSrcAlpha)
            .setDepthTest(false);

    d->uTex = buf->texture;
    //d->uColor = Vec4f(1, 1, 1, visibleOpacity());
    d->drawable.draw();

    GLState::pop();

    //qDebug() << "composite" << d->nextBufIndex - 1 << "has been drawn";
}

} // namespace de
