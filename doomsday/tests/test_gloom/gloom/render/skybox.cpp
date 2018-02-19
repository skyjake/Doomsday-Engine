/** @file render/skybox.cpp
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

#include "gloom/render/skybox.h"

#include <de/Drawable>

using namespace de;

namespace gloom {

DENG2_PIMPL_NOREF(SkyBox)
{
    typedef GLBufferT<Vertex3Tex2BoundsRgba> VBuf;

    Id            skyTex;
    Drawable      skyBox;
    GLUniform     uMvpMatrix{"uMvpMatrix", GLUniform::Mat4};
    float         scale = 1.f;
};

SkyBox::SkyBox() : d(new Impl)
{}

void SkyBox::setSize(float scale)
{
    d->scale = scale;
}

void SkyBox::glInit(const Context &context)
{
    Render::glInit(context);

    using VBuf = Impl::VBuf;

    auto *atlas = context.atlas;

    d->skyTex = atlas->alloc(context.images->image("sky.day"));

    // Make a sky box.
    VBuf *buf = new VBuf;
    VBuf::Builder verts;
    VBuf::Type v;

    v.texBounds   = atlas->imageRectf(d->skyTex).xywh();
    v.texCoord[1] = Vector2f(512, 512);
    v.rgba        = Vector4f(1, 1, 1, 1);

    const float gap = 4.f / (6*512);

    // Sides.
    v.pos = Vector3f(-1, -1, -1); v.texCoord[0] = Vector2f(gap,             1 - gap); verts << v;
    v.pos = Vector3f(-1,  1, -1); v.texCoord[0] = Vector2f(gap,             gap); verts << v;
    v.pos = Vector3f(-1, -1,  1); v.texCoord[0] = Vector2f(1.f/6.f,         1 - gap); verts << v;
    v.pos = Vector3f(-1,  1,  1); v.texCoord[0] = Vector2f(1.f/6.f,         gap); verts << v;
    v.pos = Vector3f( 1, -1,  1); v.texCoord[0] = Vector2f(2.f/6.f,         1 - gap); verts << v;
    v.pos = Vector3f( 1,  1,  1); v.texCoord[0] = Vector2f(2.f/6.f,         gap); verts << v;
    v.pos = Vector3f( 1, -1, -1); v.texCoord[0] = Vector2f(3.f/6.f,         1 - gap); verts << v;
    v.pos = Vector3f( 1,  1, -1); v.texCoord[0] = Vector2f(3.f/6.f,         gap); verts << v;
    v.pos = Vector3f(-1, -1, -1); v.texCoord[0] = Vector2f(4.f/6.f - gap,   1 - gap); verts << v;
    v.pos = Vector3f(-1,  1, -1); v.texCoord[0] = Vector2f(4.f/6.f - gap,   gap); verts << v << v;

    // Top cap.
    v.pos = Vector3f( 1,  1,  1); v.texCoord[0] = Vector2f(6.f/6.f - gap, 1 - gap); verts << v << v;
    v.pos = Vector3f(-1,  1,  1); v.texCoord[0] = Vector2f(5.f/6.f + gap, 1 - gap); verts << v;
    v.pos = Vector3f( 1,  1, -1); v.texCoord[0] = Vector2f(6.f/6.f - gap, gap); verts << v;
    v.pos = Vector3f(-1,  1, -1); v.texCoord[0] = Vector2f(5.f/6.f + gap, gap); verts << v << v;

    // Bottom cap.
    v.pos = Vector3f( 1, -1, -1); v.texCoord[0] = Vector2f(5.f/6.f - gap, gap); verts << v << v;
    v.pos = Vector3f(-1, -1, -1); v.texCoord[0] = Vector2f(4.f/6.f + gap, gap); verts << v;
    v.pos = Vector3f( 1, -1,  1); v.texCoord[0] = Vector2f(5.f/6.f - gap, 1 - gap); verts << v;
    v.pos = Vector3f(-1, -1,  1); v.texCoord[0] = Vector2f(4.f/6.f + gap, 1 - gap); verts << v;

    buf->setVertices(gl::TriangleStrip, verts, gl::Static);
    d->skyBox.addBuffer(buf);

    context.shaders->build(d->skyBox.program(), "indirect.textured.color")
            << d->uMvpMatrix
            << context.uAtlas;
}

void SkyBox::glDeinit()
{
    d->skyBox.clear();
    Render::glDeinit();
}

void SkyBox::render()
{
    GLState::push().setDepthWrite(false);

    DENG2_ASSERT(d->skyBox.program().isReady());

    d->uMvpMatrix = context().view.uMvpMatrix.toMatrix4f() *
                    Matrix4f::translate(context().view.camera->cameraPosition()) *
                    Matrix4f::scale(d->scale);
    d->skyBox.draw();

    GLState::pop();
}

} // namespace gloom
