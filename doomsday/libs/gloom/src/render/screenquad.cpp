#include <memory>

/** @file screenquad.cpp
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "gloom/render/screenquad.h"

using namespace de;

namespace gloom {

static const int BUF_ID = 1;

using VBuf = GLBufferT<Vertex2Tex>;
static std::shared_ptr<VBuf> s_vertexBuffer; // shared between all ScreenQuad instances

DE_PIMPL_NOREF(ScreenQuad)
{
    Drawable drawable;
    GLState state;

    Impl()
    {
        state.setBlend(false);
        state.setCull(gfx::None);
        state.setDepthTest(false);
        state.setDepthWrite(false);
    }
};

ScreenQuad::ScreenQuad()
    : d(new Impl)
{}

void ScreenQuad::glInit(Context &context)
{
    Render::glInit(context);

    if (!s_vertexBuffer)
    {
        s_vertexBuffer = std::make_shared<VBuf>();
        s_vertexBuffer->setVertices(
            gfx::TriangleStrip,
            VBuf::Builder().makeQuad(Rectanglef(-1, -1, 2, 2), Rectanglef(0, 0, 1, 1)),
            gfx::Static);
    }
    d->drawable.addBuffer(BUF_ID, s_vertexBuffer);
    d->drawable.setState(BUF_ID, d->state);
}

void ScreenQuad::glDeinit()
{
    d->drawable.clear();
    if (s_vertexBuffer.use_count() == 1)
    {
        s_vertexBuffer.reset();
    }
    Render::glDeinit();
}

void ScreenQuad::render()
{
    d->state.setViewport(GLState::current().viewport());
    d->drawable.draw();
}

GLProgram &ScreenQuad::addProgram(Drawable::Id programId)
{
    auto &prog = d->drawable.addProgram(programId);
    return prog;
}

Drawable &ScreenQuad::drawable()
{
    return d->drawable;
}

GLProgram &ScreenQuad::program()
{
    return d->drawable.program();
}

GLState &ScreenQuad::state()
{
    return d->state;
}

} // namespace gloom
