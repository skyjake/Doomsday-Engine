/** @file painter.cpp  GUI painter.
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Painter"
#include "de/BaseGuiApp"

#include <de/GLDrawQueue>
#include <de/GLFramebuffer>
#include <de/GLProgram>
#include <de/GLState>

namespace de {

using namespace de::internal;

DENG2_PIMPL(Painter), public Asset
{
    GLAtlasBuffer vertexBuf;    ///< Per-frame allocations.
    GLDrawQueue queue;
    GLProgram batchProgram;
    GLUniform uMvpMatrix { "uMvpMatrix", GLUniform::Mat4 };
    Rectanglef normScissorRect;

    Impl(Public *i)
        : Base(i)
        , vertexBuf(GuiVertex::formatSpec())
    {
        vertexBuf.setUsage(gl::Dynamic);
        vertexBuf.setMaxElementCount(2048);
    }

    ~Impl()
    {
        deinit();
    }

    void init()
    {
        BaseGuiApp::shaders().build(batchProgram, "batch.guiwidget")
                << uMvpMatrix;
        self().useDefaultProgram();
        setState(true);
    }

    void deinit()
    {
        batchProgram.clear();
        setState(false);
    }
};

Painter::Painter()
    : d(new Impl(this))
{}

void Painter::init()
{
    if (!d->isReady())
    {
        d->init();
    }
}

void Painter::deinit()
{
    d->deinit();
}

void Painter::setProgram(GLProgram &program)
{
    program << d->uMvpMatrix;
    d->queue.setProgram(program);
}

void Painter::useDefaultProgram()
{
    d->queue.setProgram(d->batchProgram, "uColor", GLUniform::Vec4Array);
}

void Painter::setTexture(GLUniform &uTex)
{
    flush();
    d->batchProgram << uTex;
}

void Painter::setModelViewProjection(Matrix4f const &mvp)
{
    flush();
    d->uMvpMatrix = mvp;
}

void Painter::setNormalizedScissor(Rectanglef const &normScissorRect)
{
    DENG2_ASSERT(normScissorRect.left()   >= 0);
    DENG2_ASSERT(normScissorRect.right()  <= 1);
    DENG2_ASSERT(normScissorRect.top()    >= 0);
    DENG2_ASSERT(normScissorRect.bottom() <= 1);

    d->normScissorRect = normScissorRect;

    Rectangleui const vp = GLState::current().viewport();

    Rectangleui scis = Rectangleui(Vector2ui(normScissorRect.left()   * vp.width(),
                                             normScissorRect.top()    * vp.height()),
                                   Vector2ui(std::ceil(normScissorRect.right()  * vp.width()),
                                             std::ceil(normScissorRect.bottom() * vp.height())))
            .moved(vp.topLeft);

    scis = GLState::current().target().scaleToActiveRect(scis);

    d->queue.setScissorRect(Vector4f(scis.left(),  int(vp.height()) - scis.bottom(),
                                     scis.right(), int(vp.height()) - scis.top()));
}

Rectanglef Painter::normalizedScissor() const
{
    return d->normScissorRect;
}

void Painter::setColor(Vector4f const &color)
{
    d->queue.setBufferVector(color);
}

void Painter::setSaturation(float saturation)
{
    d->queue.setBufferSaturation(saturation);
}

void Painter::drawTriangleStrip(QVector<GuiVertex> &vertices)
{
    DENG2_ASSERT(d->isReady());
    std::unique_ptr<GLSubBuffer> sub(d->vertexBuf.alloc(dsize(vertices.size())));
    sub->setBatchVertices(d->queue.batchIndex(), dsize(vertices.size()), &vertices[0]);
    d->queue.drawBuffer(*sub); // enqueues indices to be drawn
}

void Painter::flush()
{
    DENG2_ASSERT(d->isReady());
    d->queue.flush();
    d->vertexBuf.clear();
}

AttribSpec const GuiVertex::_spec[] = {
    { AttribSpec::Position,  2, GL_FLOAT, false, sizeof(GuiVertex), 0 },
    { AttribSpec::TexCoord0, 2, GL_FLOAT, false, sizeof(GuiVertex), 2 * sizeof(float) },
    { AttribSpec::Color,     4, GL_FLOAT, false, sizeof(GuiVertex), 4 * sizeof(float) },
    { AttribSpec::Index,     1, GL_FLOAT, false, sizeof(GuiVertex), 8 * sizeof(float) },
};
LIBGUI_VERTEX_FORMAT_SPEC(GuiVertex, 9 * sizeof(float))

} // namespace de
