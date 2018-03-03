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
#include "gloom/render/gbuffer.h"

#include <de/Drawable>
#include <de/ImageFile>

using namespace de;

namespace gloom {

DENG2_PIMPL_NOREF(SkyBox)
{
    typedef GLBufferT<Vertex3> VBuf;

    GLTexture     envTex;
    Drawable      skyBox;
    GLUniform     uSkyMvpMatrix{"uSkyMvpMatrix", GLUniform::Mat4};
    float         scale = 1.f;
};

SkyBox::SkyBox() : d(new Impl)
{}

void SkyBox::setSize(float scale)
{
    d->scale = scale;
}

void SkyBox::glInit(Context &context)
{
    Render::glInit(context);

    using VBuf = Impl::VBuf;

    // Load the cube map.
    {
        const Image img = context.images->image("sky.day");
        const Image::Size size{img.width() / 6, img.height()};

        d->envTex.setFilter(gl::Linear, gl::Linear, gl::MipLinear);
        d->envTex.setImage(gl::NegativeX, img.subImage(Rectanglei(0*size.x, 0, size.x, size.y)));
        d->envTex.setImage(gl::PositiveZ, img.subImage(Rectanglei(1*size.x, 0, size.x, size.y)));
        d->envTex.setImage(gl::PositiveX, img.subImage(Rectanglei(2*size.x, 0, size.x, size.y)));
        d->envTex.setImage(gl::NegativeZ, img.subImage(Rectanglei(3*size.x, 0, size.x, size.y)));
        d->envTex.setImage(gl::NegativeY, img.subImage(Rectanglei(4*size.x, 0, size.x, size.y)));
        d->envTex.setImage(gl::PositiveY, img.subImage(Rectanglei(5*size.x, 0, size.x, size.y)));
        d->envTex.generateMipmap();
    }

    // Make a sky box.
    const VBuf::Type verts[] = {
        {{-1, -1, -1}},
        {{ 1, -1, -1}},
        {{-1,  1, -1}},
        {{ 1,  1, -1}},
        {{-1, -1,  1}},
        {{ 1, -1,  1}},
        {{-1,  1,  1}},
        {{ 1,  1,  1}}
    };

    const VBuf::Index inds[] = {
        0, 3, 2, 0, 1, 3, // -Z
        4, 6, 7, 4, 7, 5, // +Z
        0, 2, 4, 6, 4, 2, // -X
        1, 5, 3, 7, 3, 5, // +X
        0, 4, 1, 5, 1, 4, // -Y
        2, 3, 7, 2, 7, 6, // +Y
    };

    VBuf *buf = new VBuf;
    buf->setVertices(verts, sizeof(verts)/sizeof(verts[0]), gl::Static);
    buf->setIndices(gl::Triangles, sizeof(inds)/sizeof(inds[0]), inds, gl::Static);
    d->skyBox.addBuffer(buf);

    context.shaders->build(d->skyBox.program(), "gloom.sky")
            << d->uSkyMvpMatrix
            << context.uEnvIntensity
            << context.uEnvMap;

    context.uEnvMap = d->envTex;
    context.uEnvIntensity = Vec3f(5, 5, 5);
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

    d->uSkyMvpMatrix = context().view.uCameraMvpMatrix.toMat4f() *
                    Mat4f::translate(context().view.camera->cameraPosition()) *
                    Mat4f::scale(d->scale);
    d->skyBox.draw();

    GLState::pop();
}

} // namespace gloom
