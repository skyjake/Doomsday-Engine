/** @file gltarget.h  GL render target.
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

#ifndef LIBGUI_GLTARGET_H
#define LIBGUI_GLTARGET_H

#include <QImage>

#include <de/libdeng2.h>
#include <de/Asset>
#include <de/Error>
#include <de/Vector>
#include <de/Rectangle>
#include <QFlags>

#include "libgui.h"
#include "opengl.h"

namespace de {

class GLTexture;

/**
 * GL render target.
 *
 * @ingroup gl
 */
class LIBGUI_PUBLIC GLTarget : public Asset
{
public:
    /// Something is incorrect in the configuration of the contained
    /// framebuffer object. @ingroup errors
    DENG2_ERROR(ConfigError);

    enum Flag {
        Color   = 0x1,  ///< Target has a color attachment.
        Depth   = 0x2,  ///< Target has a depth attachment.
        Stencil = 0x4,  ///< Target has a stencil attachment.

        ColorDepth        = Color | Depth,
        ColorDepthStencil = Color | Depth | Stencil,
        ColorStencil      = Color | Stencil,
        DepthStencil      = Depth | Stencil,

        NoAttachments = 0,
        DefaultFlags  = ColorDepth
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    typedef Vector2ui Size;

public:
    /**
     * Constructs a default render target (the window framebuffer).
     */
    GLTarget();

    /**
     * Constructs a render target than renders onto a texture. The texture must
     * be initialized with the appropriate size beforehand.
     *
     * @param colorTarget       Target texture for Color attachment.
     * @param otherAttachments  Other supporting attachments (renderbuffers).
     */
    GLTarget(GLTexture &colorTarget, Flags const &otherAttachments = NoAttachments);

    /**
     * Constructs a render target with a texture attachment and optionally
     * other renderbuffer attachments.
     *
     * @param attachment        Where to attach the texture (color, depth, stencil).
     * @param texture           Texture to render on.
     * @param otherAttachments  Other supporting attachments (renderbuffers).
     */
    GLTarget(Flags const &attachment, GLTexture &texture,
             Flags const &otherAttachments = NoAttachments);

    //GLTarget(GLTexture *color, GLTexture *depth = 0, GLTexture *stencil = 0);

    /**
     * Constructs a render target with a specific size.
     *
     * @param size   Size of the render target.
     * @param flags  Attachments to set up.
     */
    GLTarget(Vector2ui const &size, Flags const &flags = DefaultFlags);

    Flags flags() const;

    /**
     * Changes the configuration of the render target. Any previously allocated
     * renderbuffers are released.
     *
     * @param attachment  Where to attach @a texture.
     * @param texture     Texture to render on.
     * @param otherAttachments  Other supporting attachments (renderbuffers).
     */
    void configure(Flags const &attachment, GLTexture &texture,
                   Flags const &otherAttachments = NoAttachments);

    /**
     * Activates this render target as the one where GL drawing is being done.
     */
    void glBind() const;

    /**
     * Deactivates the render target.
     */
    void glRelease() const;

    GLuint glName() const;

    Size size() const;

    /**
     * Copies the contents of the render target's color attachment to an image.
     */
    QImage toImage() const;

    /**
     * Sets the color for clearing the target (see clear()).
     *
     * @param color  Color for clearing.
     */
    void setClearColor(Vector4f const &color);

    /**
     * Clears the contents of the render target's attached buffers.
     *
     * @param attachments  Which ones to clear.
     */
    void clear(Flags const &attachments);

    /**
     * Resizes the target's attached buffers and/or textures to a new size.
     * Nothing happens if the provided size is the same as the current size. If
     * resizing occurs, the contents of all buffers/textures become undefined.
     *
     * @param size  New size for buffers and/or textures.
     */
    void resize(Size const &size);

    /**
     * Sets the subregion inside the render target where scissor and viewport
     * will be constrained to. Scissor and viewport can still be defined as if
     * the entire window was in use; the target window only applies an offset
     * and scaling to both.
     *
     * @param rect   Target window rectangle. Set a null rectangle to
     *               use the entire window (like normally).
     * @param apply  Immediately update current OpenGL state accordingly.
     */
    void setActiveRect(Rectangleui const &rect, bool applyGLState = false);

    void unsetActiveRect(bool applyGLState = false);

    Rectangleui scaleToActiveRect(Rectangleui const &rect) const;

    Rectangleui const &activeRect() const;

    bool hasActiveRect() const;

private:
    DENG2_PRIVATE(d)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(GLTarget::Flags)

} // namespace de

#endif // LIBGUI_GLTARGET_H
