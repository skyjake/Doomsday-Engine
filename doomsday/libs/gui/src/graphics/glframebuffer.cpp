/** @file gltarget.cpp  GL render target.
 *
 * Implementation does not use QGLFrameBufferObject because it does not allow
 * attaching manually created textures.
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

#include "de/GLFramebuffer"
#include "de/GLTexture"
#include "de/GLState"
#include "de/GLInfo"
#include "de/GLWindow"

#include <de/Asset>
#include <de/LogBuffer>

namespace de {

static const Vec2ui nullSize;
static GLuint       defaultFramebuffer = 0;

#if defined (DE_HAVE_COLOR_ATTACHMENTS)        
static const int    MAX_COLOR_ATTACHMENTS = 4;
#else
static const int    MAX_COLOR_ATTACHMENTS = 1;
#endif

#if (DE_OPENGL_ES == 20)
#  define GL_DRAW_FRAMEBUFFER   GL_FRAMEBUFFER
#  define GL_READ_FRAMEBUFFER   GL_FRAMEBUFFER
#  define GL_RGBA8              GL_RGBA8_OES
#endif

DE_PIMPL(GLFramebuffer)
, DE_OBSERVES(Asset, Deletion)
{
    enum AttachmentId {
        ColorBuffer0,
#if defined (DE_HAVE_COLOR_ATTACHMENTS)        
        ColorBuffer1,
        ColorBuffer2,
        ColorBuffer3,
#endif
        DepthBuffer,
        StencilBuffer,
        DepthStencilBuffer,
        MAX_ATTACHMENTS
    };

    static AttachmentId attachmentToId(GLenum atc)
    {
        switch (atc)
        {
        case GL_COLOR_ATTACHMENT0:
            return ColorBuffer0;

#if defined (DE_HAVE_COLOR_ATTACHMENTS)        
        case GL_COLOR_ATTACHMENT1:
            return ColorBuffer1;

        case GL_COLOR_ATTACHMENT2:
            return ColorBuffer2;

        case GL_COLOR_ATTACHMENT3:
            return ColorBuffer3;
#endif

        case GL_DEPTH_ATTACHMENT:
            return DepthBuffer;

        case GL_STENCIL_ATTACHMENT:
            return StencilBuffer;

#if defined (DE_HAVE_DEPTH_STENCIL_ATTACHMENT)
        case GL_DEPTH_STENCIL_ATTACHMENT:
            return DepthStencilBuffer;
#endif

        default:
            DE_ASSERT_FAIL("Invalid GLFramebuffer attachment");
            break;
        }
        return ColorBuffer0; // should not be reached
    }

    static GLenum flagsToGLAttachment(Flags flags)
    {
        DE_ASSERT(!flags.testFlag(ColorDepth));
        DE_ASSERT(!flags.testFlag(ColorDepthStencil));

        return flags == Color0?  GL_COLOR_ATTACHMENT0  :
#if defined (DE_HAVE_COLOR_ATTACHMENTS)               
               flags == Color1?  GL_COLOR_ATTACHMENT1  :
               flags == Color2?  GL_COLOR_ATTACHMENT2  :
               flags == Color3?  GL_COLOR_ATTACHMENT3  :
#endif
#if defined (DE_HAVE_DEPTH_STENCIL_ATTACHMENT)
               flags == DepthStencil? GL_DEPTH_STENCIL_ATTACHMENT :
#endif
               flags == Stencil? GL_STENCIL_ATTACHMENT :
                                 GL_DEPTH_ATTACHMENT;
    }

    GLuint      fbo;
    GLuint      renderBufs[MAX_ATTACHMENTS];
    GLTexture * bufTextures[MAX_ATTACHMENTS];
    Flags       flags;
    Flags       textureAttachment; ///< Where to attach @a texture.
    GLTexture * texture;
    Vec2ui   size;
    Vec4f    clearColor;
    Rectangleui activeRect; ///< Initially null.
    int         sampleCount;

    Impl(Public *i)
        : Base(i)
        , fbo(0)
        , flags(DefaultFlags)
        , textureAttachment(NoAttachments)
        , texture(0)
        , sampleCount(0)
    {
        zap(renderBufs);
        zap(bufTextures);
    }

    Impl(Public *i, Flags texAttachment, GLTexture &colorTexture, Flags otherAtm)
        : Base(i)
        , fbo(0)
        , flags(texAttachment | otherAtm)
        , textureAttachment(texAttachment)
        , texture(&colorTexture)
        , size(colorTexture.size())
        , sampleCount(0)
    {
        zap(renderBufs);
        zap(bufTextures);
    }

    Impl(Public *i, const Vec2ui &targetSize, Flags fboFlags)
        : Base(i)
        , fbo(0)
        , flags(fboFlags)
        , textureAttachment(NoAttachments)
        , texture(0)
        , size(targetSize)
        , sampleCount(0)
    {
        zap(renderBufs);
        zap(bufTextures);
    }

    ~Impl()
    {
        dealloc();
    }

    bool isDefault() const
    {
        return !texture && size == nullSize;
    }

    static AttachmentId flagsToAttachmentId(Flags flags)
    {
        if (flags == Color0)
        {
            return ColorBuffer0;
        }
#if defined (DE_HAVE_COLOR_ATTACHMENTS)        
        if (flags == Color1)
        {
            return ColorBuffer1;
        }
        if (flags == Color2)
        {
            return ColorBuffer2;
        }
        if (flags == Color3)
        {
            return ColorBuffer3;
        }
#endif
        if (flags == Depth)
        {
            return DepthBuffer;
        }
        if (flags == DepthStencil)
        {
            return DepthStencilBuffer;
        }
        if (flags == Stencil)
        {
            return StencilBuffer;
        }
        DE_ASSERT_FAIL("Invalid attachment flags");
        return MAX_ATTACHMENTS;
    }

    int colorAttachmentCount() const
    {    
        int count = 0;
        for (int i = 0; i < MAX_COLOR_ATTACHMENTS; ++i)
        {
            if (flags & (Color0 << i)) count++;
        }
        return count;
    }

    GLTexture *bufferTexture(Flags flags) const
    {
        auto attachId = flagsToAttachmentId(flags);
        if (attachId != MAX_ATTACHMENTS)
        {
            return bufTextures[attachId];
        }
        return nullptr;
    }

    GLuint renderBuffer(Flags flags) const
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

        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        LIBGUI_ASSERT_GL_OK();
        LOG_GL_XVERBOSE("Creating FBO %i", fbo);
    }

    void attachTexture(GLTexture &tex, GLenum attachment, int level = 0)
    {
        LOG_GL_XVERBOSE("FBO %i: glTex %i (level %i) => attachment %i",
                        fbo << tex.glName() << level << attachmentToId(attachment));

        DE_ASSERT(tex.isReady());
        if (tex.isCubeMap())
        {
#if defined (DE_OPENGL)
            glFramebufferTexture(GL_FRAMEBUFFER, attachment, tex.glName(), level);
#else
            DE_ASSERT_FAIL("Cannot attach cube map texture to framebuffer");
#endif            
        }
        else
        {
            glFramebufferTexture2D(
                GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, tex.glName(), level);
        }
        LIBGUI_ASSERT_GL_OK();

        bufTextures[attachmentToId(attachment)] = &tex;
    }

    void attachRenderbuffer(AttachmentId id, GLenum type, GLenum attachment)
    {
        DE_ASSERT(size != Vec2ui(0, 0));

        glGenRenderbuffers(1, &renderBufs[id]);
        glBindRenderbuffer(GL_RENDERBUFFER, renderBufs[id]);
        LIBGUI_ASSERT_GL_OK();

#if !defined (DE_OPENGL_ES)
        if (sampleCount > 1)
        {
            if (GLInfo::extensions().NV_framebuffer_multisample_coverage)
            {
                LOG_GL_VERBOSE("FBO %i: renderbuffer %ix%i is multisampled with %i CSAA samples => attachment %i")
                        << fbo << size.x << size.y << sampleCount
                        << attachmentToId(attachment);

                glRenderbufferStorageMultisampleCoverageNV(
                        GL_RENDERBUFFER, 8, sampleCount, type, size.x, size.y);
                LIBGUI_ASSERT_GL_OK();
            }
            else
            {
                LOG_GL_VERBOSE("FBO %i: renderbuffer %ix%i is multisampled with %i samples => attachment %i")
                        << fbo << size.x << size.y << sampleCount
                        << attachmentToId(attachment);

                //DE_ASSERT(GLInfo::extensions().EXT_framebuffer_multisample);
                glRenderbufferStorageMultisample(
                        GL_RENDERBUFFER, sampleCount, type, size.x, size.y);
                LIBGUI_ASSERT_GL_OK();
            }
        }
        else
#endif
        {
            glRenderbufferStorage(GL_RENDERBUFFER, type, size.x, size.y);
            LIBGUI_ASSERT_GL_OK();
        }

        glFramebufferRenderbuffer(
                    GL_FRAMEBUFFER, attachment, GL_RENDERBUFFER, renderBufs[id]);
        LIBGUI_ASSERT_GL_OK();
    }

    void alloc()
    {
        allocFBO();

        if (texture)
        {
            // The texture's attachment point must be unambiguously defined.
            DE_ASSERT(textureAttachment == Color0  ||
                      textureAttachment == Depth   ||
                      textureAttachment == Stencil ||
                      textureAttachment == DepthStencil);

            attachTexture(*texture,
                          textureAttachment == Color0?  GL_COLOR_ATTACHMENT0  :
                          textureAttachment == Depth?   GL_DEPTH_ATTACHMENT   :
#if defined (DE_HAVE_DEPTH_STENCIL_ATTACHMENT)
                          textureAttachment == DepthStencil? GL_DEPTH_STENCIL_ATTACHMENT :
#endif
                                                        GL_STENCIL_ATTACHMENT);
        }

        if (size != nullSize) // A non-default target: size must be specified.
        {
            allocRenderBuffers();
        }

        validate();
    }

    void allocRenderBuffers()
    {
        DE_ASSERT(size != nullSize);
        
        // Fill in all the other requested attachments.
        if (flags.testFlag(Color0) && !textureAttachment.testFlag(Color0))
        {
#if defined (DE_OPENGL_ES)
            DE_ASSERT(GLInfo::extensions().OES_rgb8_rgba8);
#endif
            LOG_GL_VERBOSE("FBO %i: color renderbuffer %s") << fbo << size.asText();
            attachRenderbuffer(ColorBuffer0, GL_RGBA8, GL_COLOR_ATTACHMENT0);
        }

        allocDepthStencilRenderBuffers();

        glBindRenderbuffer(GL_RENDERBUFFER, 0);
    }

    void allocDepthStencilRenderBuffers()
    {
#if defined (DE_HAVE_DEPTH_STENCIL_ATTACHMENT)
        if (flags.testFlag(DepthStencil) && !flags.testFlag(SeparateDepthAndStencil) &&
            (!texture || textureAttachment == Color0))
        {
            // We can use a combined depth/stencil buffer.
            LOG_GL_VERBOSE("FBO %i: depth+stencil renderbuffer %s") << fbo << size.asText();
            attachRenderbuffer(DepthStencilBuffer, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL_ATTACHMENT);
        }
        else
#endif // DE_HAVE_DEPTH_STENCIL_ATTACHMENT
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

    void deallocRenderBuffers()
    {
        glDeleteRenderbuffers(MAX_ATTACHMENTS, renderBufs);
        zap(renderBufs);
        zap(bufTextures);
    }

    void dealloc()
    {
        self().setState(NotReady);
        if (fbo)
        {
            deallocRenderBuffers();
            glDeleteFramebuffers(1, &fbo);
            fbo = 0;
        }
        zap(bufTextures);
        texture = 0;
        size = nullSize;
    }

    void deallocAndReset()
    {
        dealloc();

        textureAttachment = NoAttachments;
        flags = NoAttachments;
        sampleCount = 0;
    }

    void deallocRenderBuffer(AttachmentId id)
    {
        if (renderBufs[id])
        {
            glDeleteRenderbuffers(1, &renderBufs[id]);
            renderBufs[id] = 0;
        }
    }

    void resizeRenderBuffers(const Size &newSize)
    {
        size = newSize;

        deallocRenderBuffers();
        allocRenderBuffers();
    }

    void replace(GLenum attachment, GLTexture &newTexture)
    {
        DE_ASSERT(self().isReady()); // must already be inited
        DE_ASSERT(bufTextures[attachmentToId(attachment)] != 0); // must have an attachment already

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        attachTexture(newTexture, attachment);

        validate();
    }

    void replaceWithNewRenderBuffer(Flags attachment)
    {
        DE_ASSERT(self().isReady()); // must already be inited
        if (attachment == DepthStencil) // this supported only
        {
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);

            allocDepthStencilRenderBuffers();

            validate();
        }
    }

    void replaceWithExistingRenderBuffer(Flags attachment, GLuint renderBufId)
    {
        DE_ASSERT(self().isReady());       // must already be inited

        auto id = flagsToAttachmentId(attachment);

        renderBufs[id] = renderBufId;

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferRenderbuffer(
                    GL_FRAMEBUFFER, flagsToGLAttachment(attachment),
                    GL_RENDERBUFFER, renderBufs[id]);

        LIBGUI_ASSERT_GL_OK();

        // Restore previous target.
        GLState::current().target().glBind();
    }

    void glBind() const
    {
        DE_ASSERT(fbo);

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
        LIBGUI_ASSERT_GL_OK();

#if defined (DE_HAVE_COLOR_ATTACHMENTS)
        const int count = colorAttachmentCount();

        static const GLenum drawBufs[4] = {
            GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3
        };
        glDrawBuffers(count, drawBufs);
        LIBGUI_ASSERT_GL_OK();
#endif
    }

    void glRelease() const
    {
        LIBGUI_ASSERT_GL_OK();

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, defaultFramebuffer);
        LIBGUI_ASSERT_GL_OK();

#if defined (DE_HAVE_COLOR_ATTACHMENTS)
        glDrawBuffer(GL_BACK);
        LIBGUI_ASSERT_GL_OK();
#endif        
    }

    void validate()
    {
        if (isDefault())
        {
            self().setState(Ready);
            return;
        }

        DE_ASSERT(fbo != 0);
        
        glBind();

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            deallocAndReset();

            throw ConfigError("GLFramebuffer::validate",
                status == GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT ? "Incomplete attachments" :
                status == GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS ? "Mismatch with dimensions" :
                status == GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT ? "No images attached" :
                    stringf("Unsupported (0x%x)", status).c_str());
        }
        self().setState(Ready);

        GLState::current().target().glBind();
        LIBGUI_ASSERT_GL_OK();
    }

    void assetBeingDeleted(Asset &asset)
    {
        if (texture == &asset)
        {
            dealloc();
        }
    }
};

void GLFramebuffer::setDefaultFramebuffer(GLuint defaultFBO)
{
    defaultFramebuffer = defaultFBO;
}

GLFramebuffer::GLFramebuffer() : d(new Impl(this))
{
    setState(Ready);
}

GLFramebuffer::GLFramebuffer(GLTexture &colorTarget, Flags otherAttachments)
    : d(new Impl(this, Color0, colorTarget, otherAttachments))
{
    LOG_AS("GLFramebuffer");
    d->alloc();
}

GLFramebuffer::GLFramebuffer(Flags attachment, GLTexture &texture, Flags otherAttachments)
    : d(new Impl(this, attachment, texture, otherAttachments))
{
    LOG_AS("GLFramebuffer");
    d->alloc();
}

GLFramebuffer::GLFramebuffer(const Vec2ui &size, Flags flags)
    : d(new Impl(this, size, flags))
{
    LOG_AS("GLFramebuffer");
    d->alloc();
}

Flags GLFramebuffer::flags() const
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

    d->deallocAndReset();
    setState(Ready);
}

void GLFramebuffer::configure(const Vec2ui &size, Flags flags, int sampleCount)
{
    LOG_AS("GLFramebuffer");

    d->deallocAndReset();

    d->flags = flags;
    d->size = size;
#if defined (DE_OPENGL_ES)
    DE_UNUSED(sampleCount);
#else
    d->sampleCount = (sampleCount > 1? sampleCount : 0);
#endif

    d->allocFBO();
    d->allocRenderBuffers();
    d->validate();

    LIBGUI_ASSERT_GL_OK();
}

void GLFramebuffer::configure(GLTexture *colorTex,
                              GLTexture *depthTex,
                              GLTexture *stencilTex,
                              Flags      missingRenderBuffers)
{
    configure(List<GLTexture *>({colorTex}), depthTex, stencilTex, missingRenderBuffers);
}

void GLFramebuffer::configure(const List<GLTexture *> &colorTextures,
                              GLTexture *              depthTex,
                              GLTexture *              stencilTex,
                              Flags                    missingRenderBuffers)
{
    LOG_AS("GLFramebuffer");    

    DE_ASSERT(colorTextures.size() >= 1);
    
    GLTexture *depthStencilTex = (depthTex && stencilTex && depthTex == stencilTex ? 
                                  depthTex : nullptr);
                                  
#if !defined (DE_HAVE_DEPTH_STENCIL_ATTACHMENT)
    DE_ASSERT(depthStencilTex == nullptr);
#endif

    d->deallocAndReset();

    // Set new configuration.
    for (int i = 0; i < colorTextures.sizei(); ++i)
    {
        d->flags |= Flag(Color0 << i);
        d->size = colorTextures[i]->size();
    }
    if (depthStencilTex)
    {
        d->flags |= DepthStencil;
        d->size = depthStencilTex->size();
    }
    else
    {
        if (depthTex)
        {
            d->flags |= Depth;
            d->size = depthTex->size();
        }    
        if (stencilTex)
        {
            d->flags |= Stencil;
            d->size = stencilTex->size();
        }
    }

    d->allocFBO();

    // The color attachment.
    for (int i = 0; i < colorTextures.sizei(); ++i)
    {
        auto *colorTex = colorTextures[i];
        DE_ASSERT(colorTex->isReady());
        DE_ASSERT(d->size == colorTex->size());
        d->attachTexture(*colorTex, GL_COLOR_ATTACHMENT0 + i);
    }
    if (colorTextures.isEmpty() && missingRenderBuffers.testFlag(Color0))
    {
        d->attachRenderbuffer(Impl::ColorBuffer0, GL_RGBA8, GL_COLOR_ATTACHMENT0);
    }

    // The depth/stencil attachment(s).
#if defined (DE_HAVE_DEPTH_STENCIL_ATTACHMENT)
    if (depthStencilTex)
    {
        DE_ASSERT(depthStencilTex->isReady());
        DE_ASSERT(d->size == depthStencilTex->size());
        d->attachTexture(*depthStencilTex, GL_DEPTH_STENCIL_ATTACHMENT);
    }
    else if (missingRenderBuffers.testFlag(DepthStencil))
    {
        d->attachRenderbuffer(
            Impl::DepthStencilBuffer, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL_ATTACHMENT);
    }
#else
    if (depthTex)
    {
        DE_ASSERT(depthTex->isReady());
        DE_ASSERT(d->size == depthTex->size());
        d->attachTexture(*depthTex, GL_DEPTH_ATTACHMENT);
    }
    else if (missingRenderBuffers & Depth)
    {
        d->attachRenderbuffer(Impl::DepthBuffer, GL_DEPTH_COMPONENT24_OES, GL_DEPTH_ATTACHMENT);
    }
    
    if (stencilTex)
    {
        DE_ASSERT(stencilTex->isReady());
        DE_ASSERT(d->size == stencilTex->size());
        d->attachTexture(*stencilTex, GL_STENCIL_ATTACHMENT);
    }
    else if (missingRenderBuffers & Stencil)
    {
        d->attachRenderbuffer(Impl::StencilBuffer, GL_STENCIL_INDEX, GL_STENCIL_ATTACHMENT);
    }
#endif

    LIBGUI_ASSERT_GL_OK();

    d->validate();
}

void GLFramebuffer::configure(Flags attachment, GLTexture &texture, Flags otherAttachments)
{
    LOG_AS("GLFramebuffer");

    d->deallocAndReset();

    // Set new configuration.
    d->texture           = &texture;
    d->textureAttachment = attachment;
    d->flags             = attachment | otherAttachments;
    d->size              = texture.size();

    d->alloc();
}

void GLFramebuffer::deinit()
{
    LOG_AS("GLFramebuffer");
    d->deallocAndReset();
}

void GLFramebuffer::glBind() const
{
    LIBGUI_ASSERT_GL_OK();
    DE_ASSERT(isReady());
    if (!isReady())
    {
        //qWarning() << "GLFramebuffer: Trying to bind a not-ready FBO";
        return;
    }

    if (d->fbo)
    {
        d->glBind();
    }
    else
    {
        d->glRelease();
    }

//    const GLuint fbo = (d->fbo? d->fbo : defaultFramebuffer);

//    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
//    LIBGUI_ASSERT_GL_OK();

//    if (d->fbo)
//    {
//        static const GLenum drawBufs[4] = {
//            GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3
//        };
//        glDrawBuffers(d->colorAttachmentCount(), drawBufs);
//    }
//    else
//    {
//        glDrawBuffer(GL_BACK);
//    }
}

void GLFramebuffer::glRelease() const
{
    d->glRelease();
}

Image GLFramebuffer::toImage() const
{
    if (d->flags & Color0)
    {
        // Read the contents of the color attachment.
        Size imgSize = size();
        Image img(imgSize, Image::RGBA_8888);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, d->fbo);
        glPixelStorei(GL_PACK_ALIGNMENT, 4);
        glReadPixels(0, 0, imgSize.x, imgSize.y, GL_RGBA, GL_UNSIGNED_BYTE, img.bits());
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        return img.flipped();
    }
    return Image();
}

void GLFramebuffer::setClearColor(const Vec4f &color)
{
    d->clearColor = color;
}

void GLFramebuffer::clear(Flags attachments)
{
    DE_ASSERT(isReady());

    markAsChanged();

    // The entire framebuffer is being cleared.
    GLint oldViewport[4], scissorEnabled = 0;
    if (attachments & FullClear)
    {
        glGetIntegerv(GL_SCISSOR_TEST, &scissorEnabled);
        glGetIntegerv(GL_VIEWPORT, oldViewport);
        glViewport(0, 0, d->size.x, d->size.y);
        glDisable(GL_SCISSOR_TEST);
    }
    else
    {
        GLState::current().apply();
    }

    glBind();

    glClearColor(d->clearColor.x, d->clearColor.y, d->clearColor.z, d->clearColor.w);

    // Only clear what we have.
    const Flags which = attachments & d->flags;

#if defined (DE_HAVE_COLOR_ATTACHMENTS)
    glClear((which & (Color0 | Color1 | Color2 | Color3) ?
                               ClearBufferMask::GL_COLOR_BUFFER_BIT : ClearBufferMask::GL_NONE_BIT) |
            (which & Depth ?   ClearBufferMask::GL_DEPTH_BUFFER_BIT : ClearBufferMask::GL_NONE_BIT) |
            (which & Stencil ? ClearBufferMask::GL_STENCIL_BUFFER_BIT : ClearBufferMask::GL_NONE_BIT));
#else
    glClear((which & Color0  ? GL_COLOR_BUFFER_BIT : 0) |
            (which & Depth   ? GL_DEPTH_BUFFER_BIT : 0) |
            (which & Stencil ? GL_STENCIL_BUFFER_BIT: 0));
#endif            

    // Restore previous state.
    if (attachments & FullClear)
    {
        glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);
        if (scissorEnabled) glEnable(GL_SCISSOR_TEST);
    }
    else
    {
        GLState::current().target().glBind();
    }
}

void GLFramebuffer::resize(const Size &size)
{
    // The default target resizes itself automatically with the canvas.
    if (d->size == size || d->isDefault()) return;

    d->glBind();
    if (d->texture)
    {
        d->texture->setUndefinedImage(size, d->texture->imageFormat());
    }
    d->resizeRenderBuffers(size);
    GLState::current().target().glBind();
}

GLTexture *GLFramebuffer::attachedTexture(Flags attachment) const
{
    return d->bufferTexture(attachment);
}

GLuint GLFramebuffer::attachedRenderBuffer(Flags attachment) const
{
    return d->renderBuffer(attachment);
}

void GLFramebuffer::replaceAttachment(Flags attachment, GLTexture &texture)
{
    d->replace(d->flagsToGLAttachment(attachment), texture);
}

void GLFramebuffer::replaceAttachment(Flags attachment, GLuint renderBufferId)
{
    d->replaceWithExistingRenderBuffer(attachment, renderBufferId);
}

void GLFramebuffer::replaceWithNewRenderBuffer(Flags attachment)
{
    d->replaceWithNewRenderBuffer(attachment);
}

void GLFramebuffer::releaseAttachment(Flags attachment)
{
    d->deallocRenderBuffer(d->flagsToAttachmentId(attachment));
}

void GLFramebuffer::blit(GLFramebuffer &dest, Flags attachments, gfx::Filter filtering) const
{
    LIBGUI_ASSERT_GL_OK();

    // We will modify GL state directly, so let's save the current state.
//    GLint oldDrawFbo;
//    GLint oldReadFbo;
//    GLint oldReadBuffer;
//    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &oldDrawFbo);
//    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &oldReadFbo);
//    glGetIntegerv(GL_READ_BUFFER, &oldReadBuffer);

    GLFramebuffer *oldTarget = GLState::currentTarget();

    //glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dest.glName());
    dest.glBind();
    LIBGUI_ASSERT_GL_OK();

#if defined (DE_HAVE_BLIT_FRAMEBUFFER)

    glBindFramebuffer(GL_READ_FRAMEBUFFER, glName());
    LIBGUI_ASSERT_GL_OK();

    if (attachments & ColorAny)
    {
        glReadBuffer(attachments & Color0? GL_COLOR_ATTACHMENT0
                   : attachments & Color1? GL_COLOR_ATTACHMENT1
                   : attachments & Color2? GL_COLOR_ATTACHMENT2
                   : attachments & Color3? GL_COLOR_ATTACHMENT3
                   : GL_COLOR_ATTACHMENT0);
    }

    Flags common = d->flags & dest.flags() & attachments;

    glBlitFramebuffer(0,
                      0,
                      size().x,
                      size().y,
                      0,
                      0,
                      dest.size().x,
                      dest.size().y,
                      (common & ColorAny ? ClearBufferMask::GL_COLOR_BUFFER_BIT   : ClearBufferMask::GL_NONE_BIT) |
                      (common & Depth ?    ClearBufferMask::GL_DEPTH_BUFFER_BIT   : ClearBufferMask::GL_NONE_BIT) |
                      (common & Stencil ?  ClearBufferMask::GL_STENCIL_BUFFER_BIT : ClearBufferMask::GL_NONE_BIT),
                      filtering == gfx::Nearest ? GL_NEAREST : GL_LINEAR);
    LIBGUI_ASSERT_GL_OK();

    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

    //GLInfo::EXT_framebuffer_object()->glBindFramebufferEXT(GL_FRAMEBUFFER, 0);
    //LIBGUI_ASSERT_GL_OK();

#else

    debug("[GLFramebuffer] TODO: need to implement glBlitFramebuffer: %u -> %u", glName(), dest.glName());
    debug("\t- from texture: %d", attachedTexture(Color0));
    debug("\t- to texture: %d", dest.attachedTexture(Color0));

#endif

    dest.markAsChanged();

    if (oldTarget) oldTarget->glBind();
}

void GLFramebuffer::blit(gfx::Filter filtering) const
{
    LIBGUI_ASSERT_GL_OK();

    //qDebug() << "Blitting from" << glName() << "to" << defaultFramebuffer << size().asText();

    GLFramebuffer *oldTarget = GLState::currentTarget();

#if defined (DE_HAVE_BLIT_FRAMEBUFFER)

    glBindFramebuffer(GL_READ_FRAMEBUFFER, glName());
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, defaultFramebuffer);

    glBlitFramebuffer(
                0, 0, size().x, size().y,
                0, 0, size().x, size().y,
                GL_COLOR_BUFFER_BIT,
                filtering == gfx::Nearest? GL_NEAREST : GL_LINEAR);

    LIBGUI_ASSERT_GL_OK();

    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

#else

    debug("[GLFramebuffer] TODO: need to implement glBlitFramebuffer: %u -> 0", glName());
    debug("\t- texture: %d", attachedTexture(Color0));

#endif

    if (oldTarget) oldTarget->glBind();
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
    return GLWindow::current().pixelSize();
}

void GLFramebuffer::setActiveRect(const Rectangleui &rect, bool applyGLState)
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

Vec2f GLFramebuffer::activeRectScale() const
{
    if (!hasActiveRect())
    {
        return Vec2f(1, 1);
    }
    return Vec2f(d->activeRect.size()) / size();
}

Vec2f GLFramebuffer::activeRectNormalizedOffset() const
{
    if (!hasActiveRect())
    {
        return Vec2f(0, 0);
    }
    return Vec2f(d->activeRect.topLeft) / size();
}

Rectangleui GLFramebuffer::scaleToActiveRect(const Rectangleui &rectInTarget) const
{
    // If no sub rectangle is defined, do nothing.
    if (!hasActiveRect())
    {
        return rectInTarget;
    }

    const Vec2f scaling = activeRectScale();

    return Rectangleui(d->activeRect.left()  + scaling.x * rectInTarget.left(),
                       d->activeRect.top()   + scaling.y * rectInTarget.top(),
                       rectInTarget.width()  * scaling.x,
                       rectInTarget.height() * scaling.y);
}

const Rectangleui &GLFramebuffer::activeRect() const
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
