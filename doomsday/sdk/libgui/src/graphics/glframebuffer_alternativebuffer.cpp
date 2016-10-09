/** @file gltarget_alternativebuffer.cpp  Alternative buffer attachment for GLFramebuffer.
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

namespace de {

DENG2_PIMPL_NOREF(GLFramebuffer::AlternativeBuffer)
{
    GLFramebuffer *target;
    GLTexture *texture;
    GLFramebuffer::Flags attachment;
    GLTexture *original;

    Impl()
        : target(0)
        , texture(0)
        , attachment(GLFramebuffer::NoAttachments)
        , original(0)
    {}
};

GLFramebuffer::AlternativeBuffer::AlternativeBuffer(GLFramebuffer &target, GLTexture &texture, Flags const &attachment)
    : d(new Impl)
{
    d->target = &target;
    d->texture = &texture;
    d->attachment = attachment;
}

GLFramebuffer::AlternativeBuffer::~AlternativeBuffer()
{
    deinit();
}

bool GLFramebuffer::AlternativeBuffer::init()
{
    if (d->original)
    {
        // Already done.
        return false;
    }

    // Remember the original attachment.
    d->original = d->target->attachedTexture(d->attachment);
    DENG2_ASSERT(d->original != 0);

    // Resize the alternative buffer to match current target size.
    if (d->texture->size() != d->target->size())
    {
        if (d->attachment == GLFramebuffer::DepthStencil)
        {
            d->texture->setDepthStencilContent(d->target->size());
        }
        else
        {
            DENG2_ASSERT(!"GLFramebuffer::AlternativeBuffer does not support resizing specified attachment type");
        }
    }
    d->target->replaceAttachment(d->attachment, *d->texture);
    return true;
}

bool GLFramebuffer::AlternativeBuffer::deinit()
{
    if (!d->original) return false; // Not inited.

    d->target->replaceAttachment(d->attachment, *d->original);
    return true;
}

GLFramebuffer &GLFramebuffer::AlternativeBuffer::target() const
{
    return *d->target;
}

} // namespace de
