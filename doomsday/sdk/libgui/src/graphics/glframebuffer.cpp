/** @file gltarget.cpp  GL render target.
 *
 * Implementation does not use QGLFrameBufferObject because it does not allow
 * attaching manually created textures.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/GLFramebuffer"
#include "de/GLTexture"
#include "de/GLState"
#include "de/GLInfo"
#include "de/GLWindow"
#include <de/Asset>

namespace de {

static Vector2ui const nullSize;
static GLuint defaultFramebuffer = 0;

DENG2_PIMPL(GLFramebuffer),
DENG2_OBSERVES(Asset, Deletion)
{
    enum AttachmentId {
        ColorBuffer,
        DepthBuffer,
        StencilBuffer,
        MAX_ATTACHMENTS
    };

    static AttachmentId attachmentToId(GLenum atc)
    {
        switch (atc)
        {
        case GL_COLOR_ATTACHMENT0:
            return ColorBuffer;

        case GL_DEPTH_ATTACHMENT:
            return DepthBuffer;

        case GL_STENCIL_ATTACHMENT:
            return StencilBuffer;

        case GL_DEPTH_STENCIL_ATTACHMENT:
            return DepthBuffer;

        default:
            DENG2_ASSERT(false);
            break;
        }
        return ColorBuffer; // should not be reached
    }

    static GLenum flagsToGLAttachment(Flags const &flags)
    {
        DENG2_ASSERT(!flags.testFlag(ColorDepth));
        DENG2_ASSERT(!flags.testFlag(ColorDepthStencil));

        return flags == Color?   GL_COLOR_ATTACHMENT0  :
               flags == Depth?   GL_DEPTH_ATTACHMENT   :
               flags == Stencil? GL_STENCIL_ATTACHMENT :
                                 GL_DEPTH_STENCIL_ATTACHMENT;
    }

    GLuint fbo;
    GLuint renderBufs[MAX_ATTACHMENTS];
    GLTexture *bufTextures[MAX_ATTACHMENTS];
    Flags flags;
    Flags textureAttachment;    ///< Where to attach @a texture.
    GLTexture *texture;
    Vector2ui size;
    Vector4f clearColor;
    Rectangleui activeRect; ///< Initially null.
    int sampleCount;
    //GLFramebuffer const *proxy;

    Impl(Public *i)
        : Base(i)
        , fbo(0)
        , flags(DefaultFlags)
        , textureAttachment(NoAttachments)
        , texture(0)
        , sampleCount(0)
        //, proxy(0)
    {
        zap(renderBufs);
        zap(bufTextures);
    }

    Impl(Public *i, Flags const &texAttachment, GLTexture &colorTexture, Flags const &otherAtm)
        : Base(i)
        , fbo(0)
        , flags(texAttachment | otherAtm)
        , textureAttachment(texAttachment)
        , texture(&colorTexture)
        , size(colorTexture.size())
        , sampleCount(0)
        //, proxy(0)
    {
        zap(renderBufs);
        zap(bufTextures);
    }

    Impl(Public *i, Vector2ui const &targetSize, Flags const &fboFlags)
        : Base(i)
        , fbo(0)
        , flags(fboFlags)
        , textureAttachment(NoAttachments)
        , texture(0)
        , size(targetSize)
        , sampleCount(0)
        //, proxy(0)
    {
        zap(renderBufs);
        zap(bufTextures);
    }

    ~Impl()
    {
        release();
    }

    bool isDefault() const
    {
        return !texture && size == nullSize;
    }

    static AttachmentId flagsToAttachmentId(Flags const &flags)
    {
        if (flags == Color)
        {
            return ColorBuffer;
        }
        if (flags == DepthStencil || flags == Depth)
        {
            return DepthBuffer;
        }
        if (flags == Stencil)
        {
            return StencilBuffer;
        }
        DENG2_ASSERT(!"Invalid attachment flags");
        return MAX_ATTACHMENTS;
    }

    GLTexture *bufferTexture(Flags const &flags) const
    {
        auto attachId = flagsToAttachmentId(flags);
        if (attachId != MAX_ATTACHMENTS)
        {
            return bufTextures[attachId];
        }
        return nullptr;
    }

    GLuint renderBuffer(Flags const &flags) const
    {
        auto attachId = flagsToAttachmentId(flags);
        if (attachId != MAX_ATTACHMENTS)
        {
            return renderBufs[attachId];
        }
        return 0;
    }

    void allocFBO()
    {
        if (isDefault() || fbo) return;

        GLInfo::EXT_framebuffer_object()->glGenFramebuffersEXT(1, &fbo);
        GLInfo::EXT_framebuffer_object()->glBindFramebufferEXT(GL_FRAMEBUFFER, fbo);

        LOG_GL_XVERBOSE("Creating FBO %i") << fbo;
    }

    void attachTexture(GLTexture &tex, GLenum attachment, int level = 0)
    {
        DENG2_ASSERT(tex.isReady());

        LOG_GL_XVERBOSE("FBO %i: glTex %i (level %i) => attachment %i")
                << fbo << tex.glName() << level << attachmentToId(attachment);

        GLInfo::EXT_framebuffer_object()->glFramebufferTexture2DEXT(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, tex.glName(), level);
        LIBGUI_ASSERT_GL_OK();

        bufTextures[attachmentToId(attachment)] = &tex;
    }

    void attachRenderbuffer(AttachmentId id, GLenum type, GLenum attachment)
    {
        DENG2_ASSERT(size != Vector2ui(0, 0));

        GLInfo::EXT_framebuffer_object()->glGenRenderbuffersEXT(1, &renderBufs[id]);
        GLInfo::EXT_framebuffer_object()->glBindRenderbufferEXT(GL_RENDERBUFFER, renderBufs[id]);

        if (sampleCount > 1)
        {
            if (GLInfo::extensions().NV_framebuffer_multisample_coverage)
            {
                LOG_GL_VERBOSE("FBO %i: renderbuffer %ix%i is multisampled with %i CSAA samples => attachment %i")
                        << fbo << size.x << size.y << sampleCount
                        << attachmentToId(attachment);

                GLInfo::NV_framebuffer_multisample_coverage()->glRenderbufferStorageMultisampleCoverageNV(
                        GL_RENDERBUFFER, 8, sampleCount, type, size.x, size.y);
            }
            else
            {
                LOG_GL_VERBOSE("FBO %i: renderbuffer %ix%i is multisampled with %i samples => attachment %i")
                        << fbo << size.x << size.y << sampleCount
                        << attachmentToId(attachment);

                //DENG2_ASSERT(GLInfo::extensions().EXT_framebuffer_multisample);
                GLInfo::EXT_framebuffer_multisample()->glRenderbufferStorageMultisampleEXT(
                        GL_RENDERBUFFER, sampleCount, type, size.x, size.y);
            }
        }
        else
        {
            GLInfo::EXT_framebuffer_object()->glRenderbufferStorageEXT(GL_RENDERBUFFER, type, size.x, size.y);
        }

        GLInfo::EXT_framebuffer_object()->glFramebufferRenderbufferEXT(
                    GL_FRAMEBUFFER, attachment, GL_RENDERBUFFER, renderBufs[id]);
        LIBGUI_ASSERT_GL_OK();
    }

    void alloc()
    {
        allocFBO();

        if (texture)
        {
            // The texture's attachment point must be unambiguously defined.
            DENG2_ASSERT(textureAttachment == Color   ||
                         textureAttachment == Depth   ||
                         textureAttachment == Stencil ||
                         textureAttachment == DepthStencil);

            attachTexture(*texture,
                          textureAttachment == Color?   GL_COLOR_ATTACHMENT0  :
                          textureAttachment == Depth?   GL_DEPTH_ATTACHMENT   :
                          textureAttachment == Stencil? GL_STENCIL_ATTACHMENT :
                                                        GL_DEPTH_STENCIL_ATTACHMENT);
        }

        if (size != nullSize) // A non-default target: size must be specified.
        {
            allocRenderBuffers();
        }

        validate();
    }

    void allocRenderBuffers()
    {
        DENG2_ASSERT(size != nullSize);

        // Fill in all the other requested attachments.
        if (flags.testFlag(Color) && !textureAttachment.testFlag(Color))
        {
            /// @todo Note that for GLES, GL_RGBA8 is not supported (without an extension).
            LOG_GL_VERBOSE("FBO %i: color renderbuffer %s") << fbo << size.asText();
            attachRenderbuffer(ColorBuffer, GL_RGBA8, GL_COLOR_ATTACHMENT0);
        }

        allocDepthStencilRenderBuffers();

        GLInfo::EXT_framebuffer_object()->glBindRenderbufferEXT(GL_RENDERBUFFER, 0);
    }

    void allocDepthStencilRenderBuffers()
    {
        if (flags.testFlag(DepthStencil) && !flags.testFlag(SeparateDepthAndStencil) &&
            (!texture || textureAttachment == Color))
        {
            // We can use a combined depth/stencil buffer.
            LOG_GL_VERBOSE("FBO %i: depth+stencil renderbuffer %s") << fbo << size.asText();
            attachRenderbuffer(DepthBuffer, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL_ATTACHMENT);
        }
        else
        {
            // Separate depth and stencil, then.
            if (flags.testFlag(Depth) && !textureAttachment.testFlag(Depth))
            {
                LOG_GL_VERBOSE("FBO %i: depth renderbuffer %s") << fbo << size.asText();
                attachRenderbuffer(DepthBuffer, GL_DEPTH_COMPONENT, GL_DEPTH_ATTACHMENT);
            }
            if (flags.testFlag(Stencil) && !textureAttachment.testFlag(Stencil))
            {
                LOG_GL_VERBOSE("FBO %i: stencil renderbuffer %s") << fbo << size.asText();
                attachRenderbuffer(StencilBuffer, GL_STENCIL_INDEX, GL_STENCIL_ATTACHMENT);
            }
        }
    }

    void releaseRenderBuffers()
    {
        GLInfo::EXT_framebuffer_object()->glDeleteRenderbuffersEXT(MAX_ATTACHMENTS, renderBufs);
        zap(renderBufs);
        zap(bufTextures);
    }

    void release()
    {
        self.setState(NotReady);
        if (fbo)
        {
            releaseRenderBuffers();
            GLInfo::EXT_framebuffer_object()->glDeleteFramebuffersEXT(1, &fbo);
            fbo = 0;
        }
        zap(bufTextures);
        texture = 0;
        size = nullSize;
    }

    void releaseAndReset()
    {
        release();

        textureAttachment = NoAttachments;
        flags = NoAttachments;
        sampleCount = 0;
        //proxy = 0;
    }

    void resizeRenderBuffers(Size const &newSize)
    {
        size = newSize;

        releaseRenderBuffers();
        allocRenderBuffers();
    }

    void replace(GLenum attachment, GLTexture &newTexture)
    {
        DENG2_ASSERT(self.isReady()); // must already be inited
        DENG2_ASSERT(bufTextures[attachmentToId(attachment)] != 0); // must have an attachment already

        GLInfo::EXT_framebuffer_object()->glBindFramebufferEXT(GL_FRAMEBUFFER, fbo);
        attachTexture(newTexture, attachment);

        validate();
    }

    void replaceWithNewRenderBuffer(Flags const &attachment)
    {
        DENG2_ASSERT(self.isReady()); // must already be inited
        if (attachment == DepthStencil) // this supported only
        {
            GLInfo::EXT_framebuffer_object()->glBindFramebufferEXT(GL_FRAMEBUFFER, fbo);

            allocDepthStencilRenderBuffers();

            validate();
        }
    }

    void replaceWithExistingRenderBuffer(Flags const &attachment, GLuint renderBufId)
    {
        DENG2_ASSERT(self.isReady());       // must already be inited

        auto id = flagsToAttachmentId(attachment);

        renderBufs[id] = renderBufId;

        GLInfo::EXT_framebuffer_object()->glBindFramebufferEXT(GL_FRAMEBUFFER, fbo);
        GLInfo::EXT_framebuffer_object()->glFramebufferRenderbufferEXT(
                    GL_FRAMEBUFFER, flagsToGLAttachment(attachment),
                    GL_RENDERBUFFER, renderBufs[id]);

        LIBGUI_ASSERT_GL_OK();

        // Restore previous target.
        GLState::current().target().glBind();
    }

    void validate()
    {
        if (isDefault())
        {
            self.setState(Ready);
            return;
        }

        DENG2_ASSERT(fbo != 0);

        GLInfo::EXT_framebuffer_object()->glBindFramebufferEXT(GL_FRAMEBUFFER, fbo);
        GLenum status = GLInfo::EXT_framebuffer_object()->glCheckFramebufferStatusEXT(GL_FRAMEBUFFER);

        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            releaseAndReset();

            throw ConfigError("GLFramebuffer::validate",
                status == GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT? "Incomplete attachments" :
                status == GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS? "Mismatch with dimensions" :
                status == GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT? "No images attached" :
                                                                        QString("Unsupported (0x%1)").arg(status, 0, 16));
        }
        self.setState(Ready);

        GLState::current().target().glBind();
    }

    void assetBeingDeleted(Asset &asset)
    {
        if (texture == &asset)
        {
            release();
        }
    }

#if 0
    void updateFromProxy()
    {
        if (!proxy) return;

        /// @todo Ensure this only occurs iff the target contents have changed. -jk

#ifdef _DEBUG
        if (!flags.testFlag(Changed))
        {
            //qDebug() << "GLFramebuffer: " << fbo << "being updated from proxy without Changed flag (!)";
        }
#endif

        //if (flags.testFlag(Changed))
        {
            proxy->blit(self, ColorDepth);
            flags &= ~Changed;
        }
    }
#endif
};

void GLFramebuffer::setDefaultFramebuffer(GLuint defaultFBO)
{
    defaultFramebuffer = defaultFBO;
}

GLFramebuffer::GLFramebuffer() : d(new Impl(this))
{
    setState(Ready);
}

GLFramebuffer::GLFramebuffer(GLTexture &colorTarget, Flags const &otherAttachments)
    : d(new Impl(this, Color, colorTarget, otherAttachments))
{
    LOG_AS("GLFramebuffer");
    d->alloc();
}

GLFramebuffer::GLFramebuffer(Flags const &attachment, GLTexture &texture, Flags const &otherAttachments)
    : d(new Impl(this, attachment, texture, otherAttachments))
{
    LOG_AS("GLFramebuffer");
    d->alloc();
}

GLFramebuffer::GLFramebuffer(Vector2ui const &size, Flags const &flags)
    : d(new Impl(this, size, flags))
{
    LOG_AS("GLFramebuffer");
    d->alloc();
}

GLFramebuffer::Flags GLFramebuffer::flags() const
{
    return d->flags;
}

void GLFramebuffer::markAsChanged()
{
    d->flags |= Changed;
}

void GLFramebuffer::configure()
{
    LOG_AS("GLFramebuffer");

    d->releaseAndReset();
    setState(Ready);
}

void GLFramebuffer::configure(Vector2ui const &size, Flags const &flags, int sampleCount)
{
    LOG_AS("GLFramebuffer");

    d->releaseAndReset();

    d->flags = flags;
    d->size = size;
    d->sampleCount = (sampleCount > 1? sampleCount : 0);

    d->allocFBO();
    d->allocRenderBuffers();
    d->validate();
}

void GLFramebuffer::configure(GLTexture *colorTex, GLTexture *depthStencilTex)
{
    DENG2_ASSERT(colorTex || depthStencilTex);

    LOG_AS("GLFramebuffer");

    d->releaseAndReset();

    d->flags = ColorDepthStencil;
    d->size = (colorTex? colorTex->size() : depthStencilTex->size());

    d->allocFBO();

    // The color attachment.
    if (colorTex)
    {
        DENG2_ASSERT(colorTex->isReady());
        DENG2_ASSERT(d->size == colorTex->size());
        d->attachTexture(*colorTex, GL_COLOR_ATTACHMENT0);
    }
    else
    {
        d->attachRenderbuffer(Impl::ColorBuffer, GL_RGBA8, GL_COLOR_ATTACHMENT0);
    }

    // The depth attachment.
    if (depthStencilTex)
    {
        DENG2_ASSERT(depthStencilTex->isReady());
        DENG2_ASSERT(d->size == depthStencilTex->size());
        d->attachTexture(*depthStencilTex, GL_DEPTH_STENCIL_ATTACHMENT);
    }
    else
    {
        d->attachRenderbuffer(Impl::DepthBuffer, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL_ATTACHMENT);
    }

    d->validate();
}

void GLFramebuffer::configure(Flags const &attachment, GLTexture &texture, Flags const &otherAttachments)
{
    LOG_AS("GLFramebuffer");

    d->releaseAndReset();

    // Set new configuration.
    d->texture = &texture;
    d->textureAttachment = attachment;
    d->flags = attachment | otherAttachments;
    d->size = texture.size();

    d->alloc();
}

void GLFramebuffer::deinit()
{
    LOG_AS("GLFramebuffer");
    d->releaseAndReset();
}

void GLFramebuffer::glBind() const
{
    LIBGUI_ASSERT_GL_OK();
    DENG2_ASSERT(isReady());
    if (!isReady())
    {
        //qWarning() << "GLFramebuffer: Trying to bind a not-ready FBO";
        return;
    }

    GLuint const fbo = (d->fbo? d->fbo : defaultFramebuffer);

/*#ifdef LIBGUI_USE_GLENTRYPOINTS
    if (!glBindFramebuffer) return;
#endif*/

    /*if (d->proxy)
    {
        //qDebug() << "GLFramebuffer: binding proxy of" << d->fbo << "=>";
        d->proxy->glBind();
    }
    else*/
    {
        //DENG2_ASSERT(!d->fbo || glIsFramebuffer(d->fbo));
        /*
        if (fbo && !GLInfo::EXT_framebuffer_object()->glIsFramebufferEXT(fbo))
        {
            qWarning() << "[GLFramebuffer] WARNING! Attempting to bind FBO" << fbo
                       << "that is not a valid OpenGL FBO";
        }*/

        //qDebug() << "GLFramebuffer: binding FBO" << d->fbo;

        GLInfo::EXT_framebuffer_object()->glBindFramebufferEXT(GL_FRAMEBUFFER, fbo);
        LIBGUI_ASSERT_GL_OK();
    }
}

void GLFramebuffer::glRelease() const
{
    LIBGUI_ASSERT_GL_OK();

    GLInfo::EXT_framebuffer_object()->glBindFramebufferEXT(GL_FRAMEBUFFER, defaultFramebuffer); // both read and write FBOs
    LIBGUI_ASSERT_GL_OK();

    //d->updateFromProxy();
}

QImage GLFramebuffer::toImage() const
{
    if (!d->fbo)
    {
        return GLWindow::main().grabImage();
    }
    else if (d->flags & Color)
    {
        // Read the contents of the color attachment.
        Size imgSize = size();
        QImage img(QSize(imgSize.x, imgSize.y), QImage::Format_ARGB32);
        GLInfo::EXT_framebuffer_object()->glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, d->fbo);
        LIBGUI_GL.glPixelStorei(GL_PACK_ALIGNMENT, 4);
        LIBGUI_GL.glReadPixels(0, 0, imgSize.x, imgSize.y, GL_BGRA, GL_UNSIGNED_BYTE,
                               (GLvoid *) img.constBits());
        // Restore the stack's target.
        GLState::current().target().glBind();
        return img;
    }
    return QImage();
}

void GLFramebuffer::setClearColor(Vector4f const &color)
{
    d->clearColor = color;
}

void GLFramebuffer::clear(Flags const &attachments)
{
    DENG2_ASSERT(isReady());

    markAsChanged();

    GLState::current().apply();
    glBind();

    // Only clear what we have.
    Flags which = attachments & d->flags;

    LIBGUI_GL.glClearColor(d->clearColor.x, d->clearColor.y, d->clearColor.z, d->clearColor.w);
    LIBGUI_GL.glClear((which & Color?   GL_COLOR_BUFFER_BIT   : 0) |
                      (which & Depth?   GL_DEPTH_BUFFER_BIT   : 0) |
                      (which & Stencil? GL_STENCIL_BUFFER_BIT : 0));

    GLState::current().target().glBind();
}

void GLFramebuffer::resize(Size const &size)
{
    // The default target resizes itself automatically with the canvas.
    if (d->size == size || d->isDefault()) return;

    GLInfo::EXT_framebuffer_object()->glBindFramebufferEXT(GL_FRAMEBUFFER, d->fbo);
    if (d->texture)
    {
        d->texture->setUndefinedImage(size, d->texture->imageFormat());
    }
    d->resizeRenderBuffers(size);
    GLState::current().target().glBind();
}

GLTexture *GLFramebuffer::attachedTexture(Flags const &attachment) const
{
    return d->bufferTexture(attachment);
}

GLuint GLFramebuffer::attachedRenderBuffer(Flags const &attachment) const
{
    return d->renderBuffer(attachment);
}

void GLFramebuffer::replaceAttachment(Flags const &attachment, GLTexture &texture)
{
    d->replace(d->flagsToGLAttachment(attachment), texture);
}

void GLFramebuffer::replaceAttachment(Flags const &attachment, GLuint renderBufferId)
{
    d->replaceWithExistingRenderBuffer(attachment, renderBufferId);
}

void GLFramebuffer::replaceWithNewRenderBuffer(Flags const &attachment)
{
    d->replaceWithNewRenderBuffer(attachment);
}

void GLFramebuffer::releaseAttachment(Flags const &attachment)
{
    if (attachment & Depth)
    {
        if (d->renderBufs[Impl::DepthBuffer])
        {
            GLInfo::EXT_framebuffer_object()->glDeleteRenderbuffersEXT(1, &d->renderBufs[Impl::DepthBuffer]);
            d->renderBufs[Impl::DepthBuffer] = 0;
        }
    }
    if (attachment & Stencil)
    {
        if (d->renderBufs[Impl::StencilBuffer])
        {
            GLInfo::EXT_framebuffer_object()->glDeleteRenderbuffersEXT(1, &d->renderBufs[Impl::StencilBuffer]);
            d->renderBufs[Impl::StencilBuffer] = 0;
        }
    }
}

void GLFramebuffer::blit(GLFramebuffer &dest, Flags const &attachments, gl::Filter filtering) const
{
    LIBGUI_ASSERT_GL_OK();

    auto *fb_obj  = GLInfo::EXT_framebuffer_object();
    auto *fb_blit = GLInfo::EXT_framebuffer_blit();

    if (!fb_obj || !fb_blit)
    {
        return;
    }

    fb_obj->glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, dest.glName());
    LIBGUI_ASSERT_GL_OK();

    fb_obj->glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, glName());
    LIBGUI_ASSERT_GL_OK();

    Flags common = d->flags & dest.flags() & attachments;

    fb_blit->glBlitFramebufferEXT(
                0, 0, size().x, size().y,
                0, 0, dest.size().x, dest.size().y,
                (common.testFlag(Color)?   GL_COLOR_BUFFER_BIT   : 0) |
                (common.testFlag(Depth)?   GL_DEPTH_BUFFER_BIT   : 0) |
                (common.testFlag(Stencil)? GL_STENCIL_BUFFER_BIT : 0),
                filtering == gl::Nearest? GL_NEAREST : GL_LINEAR);
    LIBGUI_ASSERT_GL_OK();

    //GLInfo::EXT_framebuffer_object()->glBindFramebufferEXT(GL_FRAMEBUFFER, 0);
    //LIBGUI_ASSERT_GL_OK();

    dest.markAsChanged();

    GLState::current().target().glBind();
}

void GLFramebuffer::blit(gl::Filter filtering) const
{
    LIBGUI_ASSERT_GL_OK();

    //qDebug() << "Blitting from" << glName() << "to" << defaultFramebuffer << size().asText();

    GLInfo::EXT_framebuffer_object()->glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, glName());
    GLInfo::EXT_framebuffer_object()->glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, defaultFramebuffer);

    GLInfo::EXT_framebuffer_blit()->glBlitFramebufferEXT(
                0, 0, size().x, size().y,
                0, 0, size().x, size().y,
                GL_COLOR_BUFFER_BIT,
                filtering == gl::Nearest? GL_NEAREST : GL_LINEAR);

    LIBGUI_ASSERT_GL_OK();

    GLState::current().target().glBind();
}

GLuint GLFramebuffer::glName() const
{
    return d->fbo? d->fbo : defaultFramebuffer;
}

GLFramebuffer::Size GLFramebuffer::size() const
{
    if (d->texture)
    {
        return d->texture->size();
    }
    else if (d->size != nullSize)
    {
        return d->size;
    }
    //qDebug() << "FBO" << d->fbo << "size" << GLWindow::main().canvas().size().asText();
    return GLWindow::main().pixelSize();
}

void GLFramebuffer::setActiveRect(Rectangleui const &rect, bool applyGLState)
{
    d->activeRect = rect;
    if (applyGLState)
    {
        // Forcibly update viewport and scissor (and other GL state).
        GLState::considerNativeStateUndefined();
        GLState::current().apply();
    }
}

void GLFramebuffer::unsetActiveRect(bool applyGLState)
{
    setActiveRect(Rectangleui(), applyGLState);
}

Vector2f GLFramebuffer::activeRectScale() const
{
    if (!hasActiveRect())
    {
        return Vector2f(1, 1);
    }
    return Vector2f(d->activeRect.size()) / size();
}

Vector2f GLFramebuffer::activeRectNormalizedOffset() const
{
    if (!hasActiveRect())
    {
        return Vector2f(0, 0);
    }
    return Vector2f(d->activeRect.topLeft) / size();
}

Rectangleui GLFramebuffer::scaleToActiveRect(Rectangleui const &rectInTarget) const
{
    // If no sub rectangle is defined, do nothing.
    if (!hasActiveRect())
    {
        return rectInTarget;
    }

    Vector2f const scaling = activeRectScale();

    return Rectangleui(d->activeRect.left()  + scaling.x * rectInTarget.left(),
                       d->activeRect.top()   + scaling.y * rectInTarget.top(),
                       rectInTarget.width()  * scaling.x,
                       rectInTarget.height() * scaling.y);
}

Rectangleui const &GLFramebuffer::activeRect() const
{
    return d->activeRect;
}

bool GLFramebuffer::hasActiveRect() const
{
    return !d->activeRect.isNull();
}

Rectangleui GLFramebuffer::rectInUse() const
{
    if (hasActiveRect())
    {
        return activeRect();
    }
    return Rectangleui::fromSize(size());
}

} // namespace de
