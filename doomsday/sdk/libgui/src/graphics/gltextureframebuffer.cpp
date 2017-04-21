/** @file glframebuffer.cpp  GL frame buffer.
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

#include "de/GLTextureFramebuffer"
#include "de/GuiApp"

#include <de/Log>
#include <de/Drawable>
#include <de/GLInfo>
#include <de/Property>

namespace de {

DENG2_STATIC_PROPERTY(DefaultSampleCount, int)

DENG2_PIMPL(GLTextureFramebuffer)
, DENG2_OBSERVES(DefaultSampleCount, Change)
{
    Image::Format colorFormat;
    Size size;
    int _samples; ///< don't touch directly (0 == default)
    GLTexture color;
    GLTexture depthStencil;
    GLFramebuffer resolvedFbo;
    Asset texFboState;

    Impl(Public *i)
        : Base(i)
        , colorFormat(Image::RGB_888)
        , _samples(0)
    {
        pDefaultSampleCount.audienceForChange() += this;
    }

    ~Impl()
    {
        release();
    }

    int sampleCount() const
    {
        if (_samples <= 0) return pDefaultSampleCount;
        return _samples;
    }

    bool isMultisampled() const
    {
        return sampleCount() > 1;
    }

    void valueOfDefaultSampleCountChanged()
    {
        reconfigure();
    }

    void release()
    {
        color.clear();
        depthStencil.clear();
        self().deinit();
        resolvedFbo.deinit();

        texFboState.setState(NotReady);
    }

#if 0
    void configureTexturesWithFallback(GLFramebuffer &fbo)
    {
        fbo.configure(&color, &depthStencil);
        // Try a couple of different ways to set up the FBO.
        for (int attempt = 0; ; ++attempt)
        {
            String failMsg;
            try
            {
                switch (attempt)
                {
                case 0:
                    // Most preferred: render both color and depth+stencil to textures.
                    // Allows shaders to access contents of the entire framebuffer.
                    failMsg = "Texture-based framebuffer failed: %s\n"
                              "Trying again without depth/stencil texture";
                    fbo.configure(&color, &depthStencil);
                    break;

                case 1:
                    failMsg = "Color texture with unified depth/stencil renderbuffer failed: %s\n"
                              "Trying again without stencil";
                    fbo.configure(GLFramebuffer::Color, color, GLFramebuffer::DepthStencil);
                    LOG_GL_WARNING("Renderer feature unavailable: lensflare depth");
                    break;

                case 2:
                    failMsg = "Color texture with depth renderbuffer failed: %s\n"
                              "Trying again without texture buffers";
                    fbo.configure(GLFramebuffer::Color, color, GLFramebuffer::Depth);
                    LOG_GL_WARNING("Renderer features unavailable: sky mask, lensflare depth");
                    break;

                case 3:
                    failMsg = "Renderbuffer-based framebuffer failed: %s\n"
                              "Trying again without stencil";
                    fbo.configure(size, GLFramebuffer::ColorDepthStencil);
                    LOG_GL_WARNING("Renderer features unavailable: postfx, lensflare depth");
                    break;

                case 4:
                    // Final fallback: simple FBO with just color+depth renderbuffers.
                    // No postfx, no access from shaders, no sky mask.
                    fbo.configure(size, GLFramebuffer::ColorDepth);
                    LOG_GL_WARNING("Renderer features unavailable: postfx, sky mask, lensflare depth");
                    break;

                default:
                    break;
                }
                break; // success!
            }
            catch (GLFramebuffer::ConfigError const &er)
            {
                if (failMsg.isEmpty()) throw er; // Can't handle it.
                LOG_GL_NOTE(failMsg) << er.asText();
            }
        }
    }
#endif

    void reconfigure()
    {
        if (!texFboState.isReady() || size == Size()) return;

        LOGDEV_GL_VERBOSE("Reconfiguring framebuffer: %s ms:%i")
                << size.asText() << sampleCount();

        // Configure textures for the framebuffer.
        color.setUndefinedImage(size, colorFormat);
        color.setWrap(gl::ClampToEdge, gl::ClampToEdge);
        color.setFilter(gl::Nearest, gl::Linear, gl::MipNone);

        DENG2_ASSERT(color.isReady());

        depthStencil.setDepthStencilContent(size);
        depthStencil.setWrap(gl::ClampToEdge, gl::ClampToEdge);
        depthStencil.setFilter(gl::Nearest, gl::Nearest, gl::MipNone);

        DENG2_ASSERT(depthStencil.isReady());

        // Configure the framebuffer(s) in the best way supported by hardware.
        // The fallbacks are for older HW/drivers only, reasonably modern ones should
        // have no problem supporting either of the first choices.

        if (isMultisampled())
        {
            self().configure(size, ColorDepthStencil, sampleCount());
            resolvedFbo.configure(&color, &depthStencil);
        }
        else
        {
            try
            {
                self().configure(&color, &depthStencil);
                resolvedFbo.setState(NotReady);
            }
            catch (ConfigError const &er)
            {
                try
                {
                    LOG_GL_WARNING("Using framebuffer configuration fallback 1 "
                                   "(depth & stencil will be used for rendering but "
                                   "are inaccessible in shaders): ") << er.asText();

                    self().configure(size, ColorDepthStencil);
                    resolvedFbo.configure(Color, color);
                }
                catch (ConfigError const &er)
                {
                    try
                    {
                        LOG_GL_WARNING("Using framebuffer configuration fallback 2 "
                                       "(only depth used for rendering, depth & stencil "
                                       "inaccessible in shaders): ") << er.asText();

                        self().configure(Color, color, Depth);
                        resolvedFbo.setState(NotReady);
                    }
                    catch (ConfigError const &er)
                    {
                        LOG_GL_WARNING("Using final framebuffer configuration fallback 3 "
                                       "(only depth used for rendering, depth & stencil "
                                       "inaccessible in shaders): ") << er.asText();
                        self().configure(size, ColorDepth);
                        resolvedFbo.configure(GLFramebuffer::Color, color);
                    }
                }
            }
        }

        self().clear(ColorDepthStencil);
        if (resolvedFbo.isReady()) resolvedFbo.clear(ColorDepthStencil);

        LIBGUI_ASSERT_GL_OK();
    }

    void resize(Size const &newSize)
    {
        if (size != newSize)
        {
            size = newSize;
            reconfigure();
        }
    }
};

GLTextureFramebuffer::GLTextureFramebuffer(Image::Format const &colorFormat, Size const &initialSize, int sampleCount)
    : d(new Impl(this))
{
    d->colorFormat = colorFormat;
    d->size        = initialSize;
    d->_samples    = sampleCount;
}

bool GLTextureFramebuffer::areTexturesReady() const
{
    return d->texFboState.isReady();
}

void GLTextureFramebuffer::glInit()
{
    if (d->texFboState.isReady()) return;

    LOG_AS("GLFramebuffer");

    d->texFboState.setState(Ready);
    d->reconfigure();
}

void GLTextureFramebuffer::glDeinit()
{
    d->release();
}

void GLTextureFramebuffer::setSampleCount(int sampleCount)
{
    if (!GLInfo::isFramebufferMultisamplingSupported())
    {
        sampleCount = 1;
    }

    if (d->_samples != sampleCount)
    {
        LOG_AS("GLFramebuffer");

        d->_samples = sampleCount;
        d->reconfigure();
    }
}

void GLTextureFramebuffer::setColorFormat(Image::Format const &colorFormat)
{
    if (d->colorFormat != colorFormat)
    {
        d->colorFormat = colorFormat;
        d->reconfigure();
    }
}

void GLTextureFramebuffer::resize(Size const &newSize)
{
    d->resize(newSize);
}

void GLTextureFramebuffer::resolveSamples()
{
    //if (d->isMultisampled())
    if (d->resolvedFbo.isReady())
    {
        // Copy the framebuffer contents to the textures (that have no multisampling).
        blit(d->resolvedFbo, ColorDepthStencil);
    }
}

GLFramebuffer &GLTextureFramebuffer::resolvedFramebuffer()
{
    if (d->resolvedFbo.isReady())
    {
        return d->resolvedFbo;
    }
    return *this;
}

GLTextureFramebuffer::Size GLTextureFramebuffer::size() const
{
    return d->size;
}

GLTexture &GLTextureFramebuffer::colorTexture() const
{
    return d->color;
}

GLTexture &GLTextureFramebuffer::depthStencilTexture() const
{
    return d->depthStencil;
}

int GLTextureFramebuffer::sampleCount() const
{
    return d->sampleCount();
}

GLTexture *GLTextureFramebuffer::attachedTexture(Flags const &attachment) const
{
    if (d->resolvedFbo.isReady())
    {
        return d->resolvedFbo.attachedTexture(attachment);
    }
    return GLFramebuffer::attachedTexture(attachment);
}

/*void GLTextureFramebuffer::clear(GLFramebuffer::Flags const &attachments)
{
    d->framebuf.clear(attachments);
}*/

/*void GLTextureFramebuffer::blit(GLFramebuffer const &target) const
{
    GLInfo::EXT_framebuffer_object()->glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, glName());
    GLInfo::EXT_framebuffer_object()->glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, target.glName());

    GLInfo::EXT_framebuffer_blit()->glBlitFramebufferEXT(
                0, 0, size().x, size().y,
                0, 0, target.size().x, target.size().y,
                GL_COLOR_BUFFER_BIT, GL_LINEAR);

    GLInfo::EXT_framebuffer_object()->glBindFramebufferEXT(GL_FRAMEBUFFER, 0);
}*/

/*void GLTextureFramebuffer::swapBuffers(Canvas &canvas, gl::SwapBufferMode swapMode)
{
    d->swapBuffers(canvas, swapMode);
}*/

/*void GLTextureFramebuffer::drawBuffer(float opacity)
{
    d->uColor = Vector4f(1, 1, 1, opacity);
    GLState::push()
            .setCull(gl::None)
            .setDepthTest(false)
            .setDepthWrite(false);
    d->drawSwap();
    GLState::pop();
    d->uColor = Vector4f(1, 1, 1, 1);
}*/

bool GLTextureFramebuffer::setDefaultMultisampling(int sampleCount)
{
    LOG_AS("GLFramebuffer");

    int const newCount = max(1, sampleCount);
    if (pDefaultSampleCount != newCount)
    {
        pDefaultSampleCount = newCount;
        return true;
    }
    return false;
}

int GLTextureFramebuffer::defaultMultisampling()
{
    return pDefaultSampleCount;
}

} // namespace de
