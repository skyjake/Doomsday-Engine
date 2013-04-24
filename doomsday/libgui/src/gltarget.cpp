/** @file gltarget.cpp  GL render target.
 *
 * Implementation does not use QGLFrameBufferObject because it does not allow
 * attaching manually created textures.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#include "de/GLTarget"
#include "de/GLTexture"
#include "de/GLState"
#include "de/PersistentCanvasWindow"
#include <de/Asset>

namespace de {

static Vector2ui const nullSize;

DENG2_PIMPL(GLTarget),
DENG2_OBSERVES(Asset, Deletion)
{
    enum RenderBufId {
        ColorBuffer,
        DepthBuffer,
        StencilBuffer,
        MAX_BUFFERS
    };

    GLuint fbo;
    GLuint renderBufs[MAX_BUFFERS];
    Flags flags;
    GLTexture *texture;
    Vector2ui size;

    Instance(Public *i)
        : Base(i), fbo(0), flags(DefaultFlags), texture(0)
    {
        zap(renderBufs);
    }

    Instance(Public *i, Flag attachment, GLTexture &colorTexture)
        : Base(i), fbo(0), flags(attachment), texture(&colorTexture)
    {
        zap(renderBufs);
    }

    Instance(Public *i, Vector2ui const &targetSize, Flags const &fboFlags)
        : Base(i), fbo(0), flags(fboFlags), texture(0), size(targetSize)
    {
        zap(renderBufs);
    }

    ~Instance()
    {
        release();
    }

    bool isDefault() const
    {
        return !texture && size == nullSize;
    }

    void alloc()
    {
        if(isDefault() || fbo) return;

        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        if(texture)
        {
            // Target renders to texture.
            DENG2_ASSERT(texture->isReady());

            glFramebufferTexture2D(GL_FRAMEBUFFER,
                                   flags & Color? GL_COLOR_ATTACHMENT0 :
                                   flags & Depth? GL_DEPTH_ATTACHMENT :
                                                  GL_STENCIL_ATTACHMENT,
                                   GL_TEXTURE_2D, texture->glName(), 0);
        }
        else if(size != nullSize)
        {
            // Target consists of one or more renderbuffers.
            if(flags & Color)
            {
                glGenRenderbuffers       (1, &renderBufs[ColorBuffer]);
                glBindRenderbuffer       (GL_RENDERBUFFER, renderBufs[ColorBuffer]);
                /// @todo Note that for GLES, GL_RGBA8 is not supported.
                glRenderbufferStorage    (GL_RENDERBUFFER, GL_RGBA8, size.x, size.y);
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          GL_RENDERBUFFER, renderBufs[ColorBuffer]);
            }
            if(flags & Depth)
            {
                glGenRenderbuffers       (1, &renderBufs[DepthBuffer]);
                glBindRenderbuffer       (GL_RENDERBUFFER, renderBufs[DepthBuffer]);
                glRenderbufferStorage    (GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, size.x, size.y);
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                          GL_RENDERBUFFER, renderBufs[DepthBuffer]);
            }
            if(flags & Stencil)
            {
                glGenRenderbuffers       (1, &renderBufs[StencilBuffer]);
                glBindRenderbuffer       (GL_RENDERBUFFER, renderBufs[StencilBuffer]);
                glRenderbufferStorage    (GL_RENDERBUFFER, GL_STENCIL_INDEX8, size.x, size.y);
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                                          GL_RENDERBUFFER, renderBufs[StencilBuffer]);
            }
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
        }

        validate();
    }

    void release()
    {
        self.setState(NotReady);
        if(fbo)
        {
            glDeleteFramebuffers(1, &fbo);
            glDeleteRenderbuffers(3, renderBufs);
        }
        texture = 0;
        size    = Vector2ui(0, 0);
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

        GLState::top().target().glBind();

        if(status != GL_FRAMEBUFFER_COMPLETE)
        {
            self.setState(NotReady);
            throw ConfigError("GLTarget::validate",
                status == GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT? "Incomplete attachments" :
                status == GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS? "Mismatch with dimensions" :
                status == GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT? "No images attached" :
                                                                        "Unsupported");
        }
        self.setState(Ready);
    }

    void assetDeleted(Asset &asset)
    {
        if(&asset == texture)
        {
            release();
        }
    }
};

GLTarget::GLTarget() : d(new Instance(this))
{
    setState(Ready);
}

GLTarget::GLTarget(GLTexture &colorTarget) : d(new Instance(this, Color, colorTarget))
{
    d->alloc();
}

GLTarget::GLTarget(Flag attachment, GLTexture &texture)
    : d(new Instance(this, attachment, texture))
{
    d->alloc();
}

GLTarget::GLTarget(Vector2ui const &size, Flags const &flags)
    : d(new Instance(this, size, flags))
{
    d->alloc();
}

void GLTarget::glBind() const
{
    if(!isReady()) return;

    glBindFramebuffer(GL_FRAMEBUFFER, d->fbo);
}

void GLTarget::glRelease() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

QImage GLTarget::toImage() const
{
    if(!d->fbo)
    {
        return PersistentCanvasWindow::main().canvas().grabImage();
    }
    else if(d->flags & Color)
    {
        // Read the contents of the color attachment.
        Size imgSize = size();
        QImage img(QSize(imgSize.x, imgSize.y), QImage::Format_ARGB32);
        glBind();
        glPixelStorei(GL_PACK_ALIGNMENT, 4);
        glReadPixels(0, 0, imgSize.x, imgSize.y, GL_RGBA, GL_UNSIGNED_BYTE, img.bits());
        // Restore the stack's target.
        GLState::top().target().glBind();
        return img;
    }
    return QImage();
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
    return PersistentCanvasWindow::main().canvas().size();
}

} // namespace de
