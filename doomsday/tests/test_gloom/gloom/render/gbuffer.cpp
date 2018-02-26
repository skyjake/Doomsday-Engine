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

#include <de/Drawable>
#include <de/GLTextureFramebuffer>
#include <de/GLProgram>
#include <de/GLState>
#include <de/GLUniform>

using namespace de;

namespace gloom {

DENG2_PIMPL(GBuffer)
{
    //ScreenQuad quad;
    GLTextureFramebuffer frame{GLTextureFramebuffer::Formats({Image::RGBA_8888,  // albedo
                                                              Image::RGB_32f,    // normals
                                                              Image::RGB_16f})}; // emissive
    GLUniform uGBufferAlbedo    {"uGBufferAlbedo",     GLUniform::Sampler2D};
    GLUniform uGBufferEmissive  {"uGBufferEmissive",   GLUniform::Sampler2D};
    GLUniform uGBufferNormal    {"uGBufferNormal",     GLUniform::Sampler2D};
    GLUniform uGBufferDepth     {"uGBufferDepth",      GLUniform::Sampler2D};
//    GLUniform uDebugMode        {"uDebugMode",         GLUniform::Int};

    Impl(Public *i) : Base(i)
    {}

    void setSize(const Vec2ui &size)
    {
        frame.resize(size);
        updateUniforms();
    }

    void updateUniforms()
    {
        uGBufferAlbedo   = frame.attachedTexture(GLFramebuffer::Color0);
        uGBufferNormal   = frame.attachedTexture(GLFramebuffer::Color1);
        uGBufferEmissive = frame.attachedTexture(GLFramebuffer::Color2);
        uGBufferDepth    = frame.attachedTexture(GLFramebuffer::DepthStencil);
    }
};

GBuffer::GBuffer()
    : d(new Impl(this))
{}

void GBuffer::glInit(Context &context)
{
    Render::glInit(context);

//    d->quad.glInit(context);
//    context.shaders->build(d->quad.program(), "gloom.finalize")
//        << context.view.uInverseProjMatrix
//        << d->uGBufferAlbedo
//        << d->uGBufferEmissive
//        << d->uGBufferNormal
//        << d->uGBufferDepth
//        << context.ssao->uSSAOBuf()
//        //<< d->uShadowMap
//        << d->uDebugMode
//        //<< d->uViewToLightMatrix
//        << context.lights->uViewSpaceLightDir();

    d->frame.glInit();
    d->updateUniforms();
}

void GBuffer::glDeinit()
{
//    d->quad.glDeinit();
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
{
//    d->quad.state().setTarget(GLState::current().target());
//    d->quad.render();
}

/*void gloom::GBuffer::setDebugMode(int debugMode)
{
    LOG_AS("GBuffer");
    LOG_MSG("Changing debug mode: %i") << debugMode;

    d->uDebugMode = debugMode;
}*/

GLFramebuffer &GBuffer::framebuf()
{
    return d->frame;
}

GLUniform &GBuffer::uGBufferAlbedo()
{
    return d->uGBufferAlbedo;
}

GLUniform &GBuffer::uGBufferEmissive()
{
    return d->uGBufferEmissive;
}

GLUniform &GBuffer::uGBufferNormal()
{
    return d->uGBufferNormal;
}

GLUniform &GBuffer::uGBufferDepth()
{
    return d->uGBufferDepth;
}

//GLUniform &GBuffer::uDebugMode()
//{
//    return d->uDebugMode;
//}

} // namespace gloom
