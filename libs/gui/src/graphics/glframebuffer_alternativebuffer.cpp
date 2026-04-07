/** @file gltarget_alternativebuffer.cpp  Alternative buffer attachment for GLFramebuffer.
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

#if 0

#include "de/glframebuffer.h"
#include "de/gltexture.h"
#include "de/glinfo.h"

namespace de {

DE_PIMPL_NOREF(GLFramebuffer::AlternativeBuffer)
{
    GLFramebuffer *target;
    GLTexture *texture;
    GLFramebuffer::Flags attachment;
    GLTexture *original;
    GLuint originalRendBuf;

    Impl()
        : target(nullptr)
        , texture(nullptr)
        , attachment(GLFramebuffer::NoAttachments)
        , original(nullptr)
        , originalRendBuf(0)
    {}
};

GLFramebuffer::AlternativeBuffer::AlternativeBuffer(GLFramebuffer &target, GLTexture &texture, const Flags &attachment)
    : d(new Impl)
{
    d->target = &target;
    d->texture = &texture;
    d->attachment = attachment;
}

GLFramebuffer::AlternativeBuffer::AlternativeBuffer(GLFramebuffer &target, const Flags &attachment)
    : d(new Impl)
{
    d->target = &target;
    d->attachment = attachment;
}

GLFramebuffer::AlternativeBuffer::~AlternativeBuffer()
{
    deinit();
}

bool GLFramebuffer::AlternativeBuffer::init()
{
    if (d->attachment != GLFramebuffer::DepthStencil)
    {
        DE_ASSERT_FAIL("GLFramebuffer::AlternativeBuffer only supports DepthStencil attachments");
        return false;
    }

    if (d->original || d->originalRendBuf)
    {
        // Already done.
        return false;
    }

    if (d->texture)
    {
        // Remember the original attachment.
        d->original = d->target->attachedTexture(d->attachment);
        DE_ASSERT(d->original != 0);

        // Resize the alternative buffer to match current target size.
        if (d->texture->size() != d->target->size())
        {
            d->texture->setDepthStencilContent(d->target->size());
        }
        d->target->replaceAttachment(d->attachment, *d->texture);
    }
    else
    {
        // Remember the original attachment.
        d->originalRendBuf = d->target->attachedRenderBuffer(d->attachment);
        if (d->originalRendBuf == 0)
        {
            // Currently using a texture attachment?
            d->original = d->target->attachedTexture(d->attachment);
            if (!d->original) return false; // Not supported.
        }

        d->target->replaceWithNewRenderBuffer(d->attachment);
    }

    return true;
}

bool GLFramebuffer::AlternativeBuffer::deinit()
{
    if (!d->original && !d->originalRendBuf) return false; // Not inited.

    if (!d->texture)
    {
        // Delete the temporary render buffer that was created in init().
        d->target->releaseAttachment(d->attachment);
    }

    // Replace the original attachment.
    if (d->original)
    {
        d->target->replaceAttachment(d->attachment, *d->original);
    }
    else if (d->originalRendBuf)
    {
        d->target->replaceAttachment(d->attachment, d->originalRendBuf);
    }

    return true;
}

GLFramebuffer &GLFramebuffer::AlternativeBuffer::target() const
{
    return *d->target;
}

} // namespace de

#endif
