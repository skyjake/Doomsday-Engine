/** @file gbuffer.cpp
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

#include "gloom/render/gbuffer.h"
#include "gloom/render/screenquad.h"
#include "gloom/render/ssao.h"
#include "gloom/render/lightrender.h"

#include <de/drawable.h>
#include <de/gltextureframebuffer.h>
#include <de/glprogram.h>
#include <de/glstate.h>
#include <de/gluniform.h>

using namespace de;

namespace gloom {

DE_PIMPL(GBuffer)
{
    GLTextureFramebuffer frame{GLTextureFramebuffer::Formats({Image::RGB_888,   // diffuse
                                                              Image::RGB_32f,   // normals (viewspace)
                                                              Image::RGB_16f,   // emissive
                                                              Image::RGBA_8888 })}; // specular/gloss
    //GLUniform uGBufferMaterial  {"uGBufferMaterial",   GLUniform::Sampler2D};
    GLUniform uGBufferDiffuse   {"uGBufferDiffuse",    GLUniform::Sampler2D};
    GLUniform uGBufferNormal    {"uGBufferNormal",     GLUniform::Sampler2D};
    GLUniform uGBufferEmissive  {"uGBufferEmissive",   GLUniform::Sampler2D};
    GLUniform uGBufferSpecGloss {"uGBufferSpecGloss",  GLUniform::Sampler2D};
    GLUniform uGBufferDepth     {"uGBufferDepth",      GLUniform::Sampler2D};

    GLUniform uViewportSize     {"uViewportSize",      GLUniform::Vec2};

    Impl(Public *i) : Base(i)
    {}

    void setSize(const Vec2ui &size)
    {
        frame.resize(size);
        updateUniforms();
    }

    void updateUniforms()
    {
        uGBufferDiffuse   = frame.attachedTexture(GLFramebuffer::Color0);
        uGBufferNormal    = frame.attachedTexture(GLFramebuffer::Color1);
        uGBufferEmissive  = frame.attachedTexture(GLFramebuffer::Color2);
        uGBufferSpecGloss = frame.attachedTexture(GLFramebuffer::Color3);
        uGBufferDepth     = frame.attachedTexture(GLFramebuffer::DepthStencil);

        uViewportSize = frame.size();
    }
};

GBuffer::GBuffer()
    : d(new Impl(this))
{}

void GBuffer::glInit(Context &context)
{
    Render::glInit(context);

    d->frame.glInit();
    d->updateUniforms();
}

void GBuffer::glDeinit()
{
    d->frame.glDeinit();
    Render::glDeinit();
}

void GBuffer::resize(const Vec2ui &size)
{
    d->setSize(size);
}

Vec2ui GBuffer::size() const
{
    return d->frame.size();
}

void GBuffer::clear()
{
    d->frame.clear(GLFramebuffer::ColorAny | GLFramebuffer::DepthStencil);
}

void GBuffer::render()
{}

GLFramebuffer &GBuffer::framebuf()
{
    return d->frame;
}

//GLUniform &GBuffer::uGBufferMaterial()
//{
//    return d->uGBufferMaterial;
//}

GLUniform &GBuffer::uGBufferDiffuse()
{
    return d->uGBufferDiffuse;
}

GLUniform &GBuffer::uGBufferEmissive()
{
    return d->uGBufferEmissive;
}

GLUniform &GBuffer::uGBufferSpecGloss()
{
    return d->uGBufferSpecGloss;
}

GLUniform &GBuffer::uGBufferNormal()
{
    return d->uGBufferNormal;
}

GLUniform &GBuffer::uGBufferDepth()
{
    return d->uGBufferDepth;
}

GLUniform &GBuffer::uViewportSize()
{
    return d->uViewportSize;
}

} // namespace gloom
