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

#include "de/GLTarget"
#include "de/GLTexture"
#include "de/GLState"
#include "de/GLInfo"
#include "de/CanvasWindow"
#include <de/Asset>

namespace de {

static Vector2ui const nullSize;

DENG2_PIMPL(GLTarget),
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
        switch(atc)
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
    GLTarget const *proxy;

    Instance(Public *i)
        : Base(i)
        , fbo(0)
        , flags(DefaultFlags)
        , textureAttachment(NoAttachments)
        , texture(0)
        , sampleCount(0)
        , proxy(0)
    {
        zap(renderBufs);
        zap(bufTextures);
    }

    Instance(Public *i, Flags const &texAttachment, GLTexture &colorTexture, Flags const &otherAtm)
        : Base(i)
        , fbo(0)
        , flags(texAttachment | otherAtm)
        , textureAttachment(texAttachment)
        , texture(&colorTexture)
        , size(colorTexture.size())
        , sampleCount(0)
        , proxy(0)
    {
        zap(renderBufs);
        zap(bufTextures);
    }

    Instance(Public *i, Vector2ui const &targetSize, Flags const &fboFlags)
        : Base(i)
        , fbo(0)
        , flags(fboFlags)
        , textureAttachment(NoAttachments)
        , texture(0)
        , size(targetSize)
        , sampleCount(0)
        , proxy(0)
    {
        zap(renderBufs);
        zap(bufTextures);
    }

    ~Instance()
    {
        release();
    }

    bool isDefault() const
    {
        return !texture && size == nullSize;
    }

    GLTexture *bufferTexture(Flags const &flags) const
    {
        if(flags == Color)
        {
            return bufTextures[ColorBuffer];
        }
        if(flags == DepthStencil || flags == Depth)
        {
            return bufTextures[DepthBuffer];
        }
        if(flags == Stencil)
        {
            return bufTextures[StencilBuffer];
        }
        return 0;
    }

    void allocFBO()
    {
        if(isDefault() || fbo) return;

        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        LOG_GL_XVERBOSE("Creating FBO %i") << fbo;
    }

    void attachTexture(GLTexture &tex, GLenum attachment, int level = 0)
    {
        DENG2_ASSERT(tex.isReady());

        LOG_GL_XVERBOSE("FBO %i: glTex %i (level %i) => attachment %i")
                << fbo << tex.glName() << level << attachmentToId(attachment);

        glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, tex.glName(), level);
        LIBGUI_ASSERT_GL_OK();

        bufTextures[attachmentToId(attachment)] = &tex;
    }

    void attachRenderbuffer(AttachmentId id, GLenum type, GLenum attachment)
    {
        DENG2_ASSERT(size != Vector2ui(0, 0));

        glGenRenderbuffers(1, &renderBufs[id]);
        glBindRenderbuffer(GL_RENDERBUFFER, renderBufs[id]);

        if(sampleCount > 1)
        {
#ifdef GL_NV_framebuffer_multisample_coverage
            if(GLInfo::extensions().NV_framebuffer_multisample_coverage)
            {
                LOG_GL_VERBOSE("FBO %i: renderbuffer %ix%i is multisampled with %i CSAA samples => attachment %i")
                        << fbo << size.x << size.y << sampleCount
                        << attachmentToId(attachment);

                glRenderbufferStorageMultisampleCoverageNV(GL_RENDERBUFFER, 8, sampleCount, type, size.x, size.y);
            }
            else
#endif
            {
                LOG_GL_VERBOSE("FBO %i: renderbuffer %i (%ix%i) is multisampled with %i samples => attachment %i (type %x)")
                        << fbo << renderBufs[id] << size.x << size.y << sampleCount
                        << attachmentToId(attachment) << type;

                //DENG2_ASSERT(GLInfo::extensions().EXT_framebuffer_multisample);
                glRenderbufferStorageMultisample(GL_RENDERBUFFER, sampleCount, type, size.x, size.y);

				LIBGUI_ASSERT_GL_OK();
            }
        }
        else
        {
            glRenderbufferStorage(GL_RENDERBUFFER, type, size.x, size.y);
        }

        glFramebufferRenderbuffer(GL_FRAMEBUFFER,  attachment,
                                  GL_RENDERBUFFER, renderBufs[id]);
        LIBGUI_ASSERT_GL_OK();
    }

    void alloc()
    {
        allocFBO();

        if(texture)
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

        if(size != nullSize) // A non-default target: size must be specified.
        {
            allocRenderBuffers();
        }

        validate();
    }

    void allocRenderBuffers()
    {
        DENG2_ASSERT(size != nullSize);

        // Fill in all the other requested attachments.
        if(flags.testFlag(Color) && !textureAttachment.testFlag(Color))
        {
            /// @todo Note that for GLES, GL_RGBA8 is not supported (without an extension).
            LOG_GL_VERBOSE("FBO %i: color renderbuffer %s") << fbo << size.asText();
            attachRenderbuffer(ColorBuffer, GL_RGBA8, GL_COLOR_ATTACHMENT0);
        }

        if(flags.testFlag(DepthStencil) && (!texture || textureAttachment == Color))
        {
            // We can use a combined depth/stencil buffer.
            LOG_GL_VERBOSE("FBO %i: depth+stencil renderbuffer %s") << fbo << size.asText();
            attachRenderbuffer(DepthBuffer, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL_ATTACHMENT);
        }
        else
        {
            // Separate depth and stencil, then.
            if(flags.testFlag(Depth) && !textureAttachment.testFlag(Depth))
            {
                LOG_GL_VERBOSE("FBO %i: depth renderbuffer %s") << fbo << size.asText();
                attachRenderbuffer(DepthBuffer, GL_DEPTH_COMPONENT16, GL_DEPTH_ATTACHMENT);
            }
            if(flags.testFlag(Stencil) && !textureAttachment.testFlag(Stencil))
            {
                LOG_GL_VERBOSE("FBO %i: stencil renderbuffer %s") << fbo << size.asText();
                attachRenderbuffer(StencilBuffer, GL_STENCIL_INDEX8, GL_STENCIL_ATTACHMENT);
            }
        }

        glBindRenderbuffer(GL_RENDERBUFFER, 0);
    }

    void releaseRenderBuffers()
    {
        glDeleteRenderbuffers(MAX_ATTACHMENTS, renderBufs);
        zap(renderBufs);
        zap(bufTextures);
    }

    void release()
    {
        self.setState(NotReady);
        if(fbo)
        {
            releaseRenderBuffers();
            glDeleteFramebuffers(1, &fbo);
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
        proxy = 0;
    }

    void resizeRenderBuffers(Size const &newSize)
    {
        size = newSize;

        releaseRenderBuffers();
        allocRenderBuffers();
    }

    void replace(GLenum attachment, GLTexture &newTexture)
    {
        DENG2_ASSERT(self.isReady());       // must already be inited
        DENG2_ASSERT(bufTextures[attachmentToId(attachment)] != 0); // must have an attachment already

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        attachTexture(newTexture, attachment);

        // Restore previous target.
        GLState::current().target().glBind();
    }

    void validate()
    {
        if(isDefault())
        {
            self.setState(Ready);
            return;
        }

        DENG2_ASSERT(fbo != 0);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        GLState::considerNativeStateUndefined(); // state was manually changed

        if(status != GL_FRAMEBUFFER_COMPLETE)
        {
            releaseAndReset();

            throw ConfigError("GLTarget::validate",
                status == GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT? "Incomplete attachments" :
                status == GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS? "Mismatch with dimensions" :
                status == GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT? "No images attached" :
                                                                        "Unsupported");
        }
        self.setState(Ready);
    }

    void assetBeingDeleted(Asset &asset)
    {
        if(texture == &asset)
        {
            release();
        }
    }

    void updateFromProxy()
    {
        if(!proxy) return;

        /// @todo Ensure this only occurs iff the target contents have changed. -jk

#ifdef _DEBUG
        if(!flags.testFlag(Changed))
        {
            //qDebug() << "GLTarget: " << fbo << "being updated from proxy without Changed flag (!)";
        }
#endif

        //if(flags.testFlag(Changed))
        {
            proxy->blit(self, ColorDepth);
            flags &= ~Changed;
        }
    }
};

GLTarget::GLTarget() : d(new Instance(this))
{
    setState(Ready);
}

GLTarget::GLTarget(GLTexture &colorTarget, Flags const &otherAttachments)
    : d(new Instance(this, Color, colorTarget, otherAttachments))
{
    LOG_AS("GLTarget");
    d->alloc();
}

GLTarget::GLTarget(Flags const &attachment, GLTexture &texture, Flags const &otherAttachments)
    : d(new Instance(this, attachment, texture, otherAttachments))
{
    LOG_AS("GLTarget");
    d->alloc();
}

GLTarget::GLTarget(Vector2ui const &size, Flags const &flags)
    : d(new Instance(this, size, flags))
{
    LOG_AS("GLTarget");
    d->alloc();
}

GLTarget::Flags GLTarget::flags() const
{
    return d->flags;
}

void GLTarget::markAsChanged()
{
    d->flags |= Changed;
}

void GLTarget::configure()
{
    LOG_AS("GLTarget");

    d->releaseAndReset();
    setState(Ready);
}

void GLTarget::configure(Vector2ui const &size, Flags const &flags, int sampleCount)
{
    LOG_AS("GLTarget");

    d->releaseAndReset();

    d->flags = flags;
    d->size = size;
    d->sampleCount = (sampleCount > 1? sampleCount : 0);

    d->allocFBO();
    d->allocRenderBuffers();
    d->validate();
}

void GLTarget::configure(GLTexture *colorTex, GLTexture *depthStencilTex)
{
    DENG2_ASSERT(colorTex || depthStencilTex);

    LOG_AS("GLTarget");

    d->releaseAndReset();

    d->flags = ColorDepthStencil;
    d->size = (colorTex? colorTex->size() : depthStencilTex->size());

    d->allocFBO();

    // The color attachment.
    if(colorTex)
    {
        DENG2_ASSERT(colorTex->isReady());
        DENG2_ASSERT(d->size == colorTex->size());
        d->attachTexture(*colorTex, GL_COLOR_ATTACHMENT0);
    }
    else
    {
        d->attachRenderbuffer(Instance::ColorBuffer, GL_RGBA8, GL_COLOR_ATTACHMENT0);
    }

    // The depth attachment.
    if(depthStencilTex)
    {
        DENG2_ASSERT(depthStencilTex->isReady());
        DENG2_ASSERT(d->size == depthStencilTex->size());
        d->attachTexture(*depthStencilTex, GL_DEPTH_STENCIL_ATTACHMENT);
    }
    else
    {
        d->attachRenderbuffer(Instance::DepthBuffer, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL_ATTACHMENT);
    }

    d->validate();
}

void GLTarget::configure(Flags const &attachment, GLTexture &texture, Flags const &otherAttachments)
{
    LOG_AS("GLTarget");

    d->releaseAndReset();

    // Set new configuration.
    d->texture = &texture;
    d->textureAttachment = attachment;
    d->flags = attachment | otherAttachments;
    d->size = texture.size();

    d->alloc();
}

void GLTarget::glBind() const
{
    LIBGUI_ASSERT_GL_OK();
    DENG2_ASSERT(isReady());
    if(!isReady()) return;

    if(d->proxy)
    {
        //qDebug() << "GLTarget: binding proxy of" << d->fbo << "=>";
        d->proxy->glBind();
    }
    else
    {
        //DENG2_ASSERT(!d->fbo || glIsFramebuffer(d->fbo));
		if(d->fbo && !glIsFramebuffer(d->fbo))
		{
			qDebug() << "GLTarget: WARNING! Attempting to bind FBO" << d->fbo 
				     << "that is not a valid OpenGL FBO";
		}

        //qDebug() << "GLTarget: binding FBO" << d->fbo;
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, d->fbo);
        LIBGUI_ASSERT_GL_OK();
    }
}

void GLTarget::glRelease() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0); // both read and write FBOs

    d->updateFromProxy();
}

QImage GLTarget::toImage() const
{
    if(!d->fbo)
    {
        return CanvasWindow::main().canvas().grabImage();
    }
    else if(d->flags & Color)
    {
        // Read the contents of the color attachment.
        Size imgSize = size();
        QImage img(QSize(imgSize.x, imgSize.y), QImage::Format_ARGB32);
        glBindFramebuffer(GL_FRAMEBUFFER, d->fbo);
        glPixelStorei(GL_PACK_ALIGNMENT, 4);
        glReadPixels(0, 0, imgSize.x, imgSize.y, GL_BGRA, GL_UNSIGNED_BYTE,
                     (GLvoid *) img.constBits());
        // Restore the stack's target.
        GLState::current().target().glBind();
        return img;
    }
    return QImage();
}

void GLTarget::setClearColor(Vector4f const &color)
{
    d->clearColor = color;
}

void GLTarget::clear(Flags const &attachments)
{
    DENG2_ASSERT(isReady());

    markAsChanged();

    GLState::current().apply();
    glBind();

    // Only clear what we have.
    Flags which = attachments & d->flags;

    glClearColor(d->clearColor.x, d->clearColor.y, d->clearColor.z, d->clearColor.w);
    glClear((which & Color?   GL_COLOR_BUFFER_BIT   : 0) |
            (which & Depth?   GL_DEPTH_BUFFER_BIT   : 0) |
            (which & Stencil? GL_STENCIL_BUFFER_BIT : 0));

    GLState::current().target().glBind();
}

void GLTarget::resize(Size const &size)
{
    // The default target resizes itself automatically with the canvas.
    if(d->size == size || d->isDefault()) return;

    glBindFramebuffer(GL_FRAMEBUFFER, d->fbo);
    if(d->texture)
    {
        d->texture->setUndefinedImage(size, d->texture->imageFormat());
    }
    d->resizeRenderBuffers(size);
    GLState::current().target().glBind();
}

GLTexture *GLTarget::attachedTexture(Flags const &attachment) const
{
    return d->bufferTexture(attachment);
}

void GLTarget::replaceAttachment(Flags const &attachment, GLTexture &texture)
{
    DENG2_ASSERT(!d->isDefault());

    d->replace(d->flagsToGLAttachment(attachment), texture);
}

void GLTarget::setProxy(GLTarget const *proxy)
{
    d->proxy = proxy;
}

void GLTarget::updateFromProxy()
{
    d->updateFromProxy();
}

void GLTarget::blit(GLTarget &dest, Flags const &attachments, gl::Filter filtering) const
{
    //qDebug() << "GLTarget: blit from" << d->fbo << "to" << dest.glName();

    /*DENG2_ASSERT(GLInfo::extensions().EXT_framebuffer_blit);
    if(!GLInfo::extensions().EXT_framebuffer_blit) return;*/

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, d->fbo);
    LIBGUI_ASSERT_GL_OK();

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dest.glName());
    LIBGUI_ASSERT_GL_OK();

    Flags common = d->flags & dest.flags() & attachments;

    glBlitFramebuffer(0, 0, size().x, size().y,
                      0, 0, dest.size().x, dest.size().y,
                      (common.testFlag(Color)?   GL_COLOR_BUFFER_BIT   : 0) |
                      (common.testFlag(Depth)?   GL_DEPTH_BUFFER_BIT   : 0) |
                      (common.testFlag(Stencil)? GL_STENCIL_BUFFER_BIT : 0),
                      filtering == gl::Nearest? GL_NEAREST : GL_LINEAR);
    LIBGUI_ASSERT_GL_OK();

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

    dest.markAsChanged();

    GLState::current().target().glBind();
}

GLuint GLTarget::glName() const
{
    return d->fbo;
}

GLTarget::Size GLTarget::size() const
{
    if(d->texture)
    {
        return d->texture->size();
    }
    else if(d->size != nullSize)
    {
        return d->size;
    }
    return CanvasWindow::main().canvas().size();
}

void GLTarget::setActiveRect(Rectangleui const &rect, bool applyGLState)
{
    d->activeRect = rect;
    if(applyGLState)
    {
        // Forcibly update viewport and scissor (and other GL state).
        GLState::considerNativeStateUndefined();
        GLState::current().apply();
    }
}

void GLTarget::unsetActiveRect(bool applyGLState)
{
    setActiveRect(Rectangleui(), applyGLState);
}

Vector2f GLTarget::activeRectScale() const
{
    if(!hasActiveRect())
    {
        return Vector2f(1, 1);
    }
    return Vector2f(d->activeRect.size()) / size();
}

Vector2f GLTarget::activeRectNormalizedOffset() const
{
    if(!hasActiveRect())
    {
        return Vector2f(0, 0);
    }
    return Vector2f(d->activeRect.topLeft) / size();
}

Rectangleui GLTarget::scaleToActiveRect(Rectangleui const &rectInTarget) const
{
    // If no sub rectangle is defined, do nothing.
    if(!hasActiveRect())
    {
        return rectInTarget;
    }

    Vector2f const scaling = activeRectScale();

    return Rectangleui(d->activeRect.left()  + scaling.x * rectInTarget.left(),
                       d->activeRect.top()   + scaling.y * rectInTarget.top(),
                       rectInTarget.width()  * scaling.x,
                       rectInTarget.height() * scaling.y);
}

Rectangleui const &GLTarget::activeRect() const
{
    return d->activeRect;
}

bool GLTarget::hasActiveRect() const
{
    return !d->activeRect.isNull();
}

Rectangleui GLTarget::rectInUse() const
{
    if(hasActiveRect())
    {
        return activeRect();
    }
    return Rectangleui::fromSize(size());
}

} // namespace de
