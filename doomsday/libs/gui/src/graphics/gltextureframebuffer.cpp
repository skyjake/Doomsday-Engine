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

#include "de/gltextureframebuffer.h"
#include "de/guiapp.h"

#include <de/log.h>
#include <de/drawable.h>
#include <de/glinfo.h>
#include <de/property.h>

namespace de {

DE_STATIC_PROPERTY(DefaultSampleCount, int)

DE_PIMPL(GLTextureFramebuffer)
, DE_OBSERVES(DefaultSampleCount, Change)
{
    struct ColorAttachment {
        Image::Format format;
        std::shared_ptr<GLTexture> texture;
    };
    Size                  size;
    int                   _samples{0}; ///< don't touch directly (0 == default)
    List<ColorAttachment> color;
    GLTexture             depthStencil;
    GLFramebuffer         resolvedFbo;
    Asset                 texFboState;

    Impl(Public *i)
        : Base(i)
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

    List<GLTexture *> colorAttachments()
    {
        List<GLTexture *> attachments;
        for (const auto &cb : color)
        {
            attachments << cb.texture.get();
        }
        return attachments;
    }

    void release()
    {
        for (auto &buf : color)
        {
            buf.texture->clear();
        }
        depthStencil.clear();
        self().deinit();
        resolvedFbo.deinit();

        texFboState.setState(NotReady);
    }

    void reconfigure()
    {
        if (!texFboState.isReady() || size == Size()) return;

        LOGDEV_GL_VERBOSE("Reconfiguring framebuffer: %s ms:%i")
                << size.asText() << sampleCount();

        // Configure textures for the framebuffer.
        for (auto &colorBuf : color)
        {
            colorBuf.texture->setUndefinedImage(size, colorBuf.format);
            colorBuf.texture->setWrap(gfx::ClampToEdge, gfx::ClampToEdge);
            colorBuf.texture->setFilter(gfx::Nearest, gfx::Linear, gfx::MipNone);

            DE_ASSERT(colorBuf.texture->isReady());
        }

        depthStencil.setDepthStencilContent(size);
        depthStencil.setWrap(gfx::ClampToEdge, gfx::ClampToEdge);
        depthStencil.setFilter(gfx::Nearest, gfx::Nearest, gfx::MipNone);

        DE_ASSERT(depthStencil.isReady());

        // Configure the framebuffer(s) in the best way supported by hardware.
        // The fallbacks are for older HW/drivers only, reasonably modern ones should
        // have no problem supporting either of the first choices.

        if (isMultisampled())
        {
            self().configure(size, ColorDepthStencil, sampleCount());
            resolvedFbo.configure(color[0].texture.get(), &depthStencil, &depthStencil);
        }
        else
        {
            try
            {
                if (color.size() == 1)
                {
                    self().configure(color[0].texture.get(), &depthStencil, &depthStencil);
                }
                else
                {
                    self().configure(colorAttachments(), &depthStencil, &depthStencil);
                }
                resolvedFbo.setState(NotReady);
            }
            catch (const ConfigError &er)
            {
                try
                {
                    LOG_GL_WARNING("Using framebuffer configuration fallback 1 "
                                   "(depth & stencil will be used for rendering but "
                                   "are inaccessible in shaders): ") << er.asText();

                    self().configure(size, ColorDepthStencil);
                    resolvedFbo.configure(Color0, *color[0].texture);
                }
                catch (const ConfigError &er)
                {
                    try
                    {
                        LOG_GL_WARNING("Using framebuffer configuration fallback 2 "
                                       "(only depth used for rendering, depth & stencil "
                                       "inaccessible in shaders): ") << er.asText();

                        self().configure(Color0, *color[0].texture, Depth);
                        resolvedFbo.setState(NotReady);
                    }
                    catch (const ConfigError &er)
                    {
                        LOG_GL_WARNING("Using final framebuffer configuration fallback 3 "
                                       "(only depth used for rendering, depth & stencil "
                                       "inaccessible in shaders): ") << er.asText();
                        self().configure(size, ColorDepth);
                        resolvedFbo.configure(Color0, *color[0].texture);
                    }
                }
            }
        }

        self().clear(ColorDepthStencil);
        if (resolvedFbo.isReady()) resolvedFbo.clear(ColorDepthStencil);

        LIBGUI_ASSERT_GL_OK();
    }

    void resize(const Size &newSize)
    {
        if (size != newSize)
        {
            size = newSize;
            reconfigure();
        }
    }
};

GLTextureFramebuffer::GLTextureFramebuffer(Image::Format colorFormat,
                                           const Size &  initialSize,
                                           int           sampleCount)
    : d(new Impl(this))
{
    d->color << Impl::ColorAttachment{colorFormat, std::make_shared<GLTexture>()};

    d->size     = initialSize;
    d->_samples = sampleCount;
}

GLTextureFramebuffer::GLTextureFramebuffer(Formats colorFormats)
    : d(new Impl(this))
{
    for (auto format : colorFormats)
    {
        d->color << Impl::ColorAttachment{format, std::make_shared<GLTexture>()};
    }
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

void GLTextureFramebuffer::setColorFormat(Image::Format colorFormat)
{
    if (d->color[0].format != colorFormat)
    {
        d->color[0].format = colorFormat;
        d->reconfigure();
    }
}

void GLTextureFramebuffer::resize(const Size &newSize)
{
    d->resize(newSize);
}

void GLTextureFramebuffer::resolveSamples()
{
    if (d->resolvedFbo.isReady())
    {
        DE_ASSERT(d->isMultisampled());
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
    return *d->color[0].texture;
}

GLTexture &GLTextureFramebuffer::depthStencilTexture() const
{
    return d->depthStencil;
}

int GLTextureFramebuffer::sampleCount() const
{
    return d->sampleCount();
}

GLTexture *GLTextureFramebuffer::attachedTexture(Flags attachment) const
{
    if (d->resolvedFbo.isReady())
    {
        return d->resolvedFbo.attachedTexture(attachment);
    }
    return GLFramebuffer::attachedTexture(attachment);
}

bool GLTextureFramebuffer::setDefaultMultisampling(int sampleCount)
{
    LOG_AS("GLFramebuffer");

    const int newCount = max(1, sampleCount);
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
