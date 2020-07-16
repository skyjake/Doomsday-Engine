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

#include "de/painter.h"
#include "de/baseguiapp.h"

#include <de/gldrawqueue.h>
#include <de/glframebuffer.h>
#include <de/glprogram.h>
#include <de/glstate.h>

namespace de {

using namespace de::internal;

DE_PIMPL(Painter), public Asset
{
    GLAtlasBuffer vertexBuf; ///< Per-frame allocations.
    GLDrawQueue   queue;
    GLProgram     batchProgram;
    GLUniform     uMvpMatrix{"uMvpMatrix", GLUniform::Mat4};
    Rectanglef    normScissorRect;

    Impl(Public *i)
        : Base(i)
        , vertexBuf(GuiVertex::formatSpec())
    {
        vertexBuf.setUsage(gfx::Dynamic);
        vertexBuf.setMaxElementCount(2048);
    }

    ~Impl()
    {
        deinit();
    }

    void init()
    {
        BaseGuiApp::shaders().build(batchProgram, "ui.guiwidget.batch")
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
    d->queue.beginFrame();
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

void Painter::setModelViewProjection(const Mat4f &mvp)
{
    flush();
    d->uMvpMatrix = mvp;
}

void Painter::setNormalizedScissor(const Rectanglef &normScissorRect)
{
    d->normScissorRect = normScissorRect & Rectanglef(0, 0, 1, 1);

    DE_ASSERT(d->normScissorRect.left()   >= 0);
    DE_ASSERT(d->normScissorRect.right()  <= 1);
    DE_ASSERT(d->normScissorRect.top()    >= 0);
    DE_ASSERT(d->normScissorRect.bottom() <= 1);

    const Rectangleui vp = GLState::current().viewport();

    Rectangleui scis = Rectangleui(Vec2ui(d->normScissorRect.left() * vp.width(),
                                          d->normScissorRect.top()  * vp.height()),
                                   Vec2ui(std::ceil(d->normScissorRect.right()  * vp.width()),
                                          std::ceil(d->normScissorRect.bottom() * vp.height())))
                           .moved(vp.topLeft);

    scis = GLState::current().target().scaleToActiveRect(scis);

    d->queue.setBatchScissorRect(Vec4f(scis.left(),  int(vp.height()) - scis.bottom(),
                                          scis.right(), int(vp.height()) - scis.top()));
}

Rectanglef Painter::normalizedScissor() const
{
    return d->normScissorRect;
}

void Painter::setColor(const Vec4f &color)
{
    d->queue.setBatchColor(color);
}

void Painter::setSaturation(float saturation)
{
    d->queue.setBatchSaturation(saturation);
}

void Painter::drawTriangleStrip(List<GuiVertex> &vertices)
{
    DE_ASSERT(d->isReady());
    std::unique_ptr<GLSubBuffer> sub(d->vertexBuf.alloc(dsize(vertices.size())));
    d->queue.setBuffer(sub->hostBuffer()); // determines final batch index
    sub->setBatchVertices(d->queue.batchIndex(), dsize(vertices.size()), &vertices[0]);
    d->queue.enqueueDraw(*sub);
}

void Painter::flush()
{
    DE_ASSERT(d->isReady());
    d->queue.flush();
    d->vertexBuf.clear();
}

AttribSpec const GuiVertex::_spec[] = {
    { AttribSpec::Position, 2, GL_FLOAT, false, sizeof(GuiVertex), 0 },
    { AttribSpec::TexCoord, 2, GL_FLOAT, false, sizeof(GuiVertex), 2 * sizeof(float) },
    { AttribSpec::Color,    4, GL_FLOAT, false, sizeof(GuiVertex), 4 * sizeof(float) },
    { AttribSpec::Index,    1, GL_FLOAT, false, sizeof(GuiVertex), 8 * sizeof(float) },
};
LIBGUI_VERTEX_FORMAT_SPEC(GuiVertex, 9 * sizeof(float))

} // namespace de
