/** @file gltarget.h  GL render target.
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
#include "../GLTexture"

namespace de {

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

        Changed = 0x8, ///< Draw/clear has occurred on the target.

        ColorDepth        = Color | Depth,
        ColorDepthStencil = Color | Depth | Stencil,
        ColorStencil      = Color | Stencil,
        DepthStencil      = Depth | Stencil,

        NoAttachments = 0,
        DefaultFlags  = ColorDepth
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    typedef Vector2ui Size;

    /**
     * Utility for temporarily using an alternative buffer as one of a render
     * target's attachments. Usage:
     * - construct as a local variable
     * - init
     * - automatically deinited when goes out of scope
     */
    class LIBGUI_PUBLIC AlternativeBuffer
    {
    public:
        /**
         * Prepares an alternative texture attachment. The new texture is not
         * taken into use yet.
         *
         * @param target      The rendering target this is for.
         * @param texture     Texture to use as alternative attachment.
         * @param attachment  Which attachment.
         */
        AlternativeBuffer(GLTarget &target, GLTexture &texture, Flags const &attachment);

        /**
         * Automatically deinitialize the alternative buffer, if it was taken
         * into use.
         */
        ~AlternativeBuffer();

        /**
         * Take the alternative buffer into use. The original buffer is
         * remembered and will be restored when the alternative buffer is no
         * longer in use. Nothing happens if this already has been called
         * previously.
         *
         * @return @c true, if initialization was done. Otherwise, @c false
         * (for example if already initialized).
         */
        bool init();

        /**
         * Restores the original attachment.
         *
         * @return @c true, if the original attachment was restored. Otherwise,
         * @c false (for example if already deinitialized).
         */
        bool deinit();

        GLTarget &target() const;

    private:
        DENG2_PRIVATE(d)
    };

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
     * Marks the rendering target modified. This is done automatically when the
     * target is cleared or when GLBuffer is used to draw something on the
     * target.
     */
    void markAsChanged();

    /**
     * Reconfigures the render target back to the default OpenGL framebuffer.
     * All attachments will be released.
     */
    void configure();

    /**
     * Configure the target with one or more renderbuffers. Multisampled buffers
     * can be configured by giving a @a sampleCount higher than 1.
     *
     * @param size         Size of the render target.
     * @param flags        Which attachments to set up.
     * @param sampleCount  Number of samples per pixel in each attachment.
     */
    void configure(Vector2ui const &size, Flags const &flags = DefaultFlags,
                   int sampleCount = 1);

    /**
     * Reconfigures the render target with two textures, one for the color
     * values and one for depth/stencil values.
     *
     * If @a colorTex or @a depthStencilTex is omitted, a renderbuffer will be
     * created in its place.
     *
     * Any previous attachments are released.
     *
     * @param colorTex         Texture for color values.
     * @param depthStencilTex  Texture for depth/stencil values.
     */
    void configure(GLTexture *colorTex, GLTexture *depthStencilTex);

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
     * Returns the texture being used for a particular attachment in this target.
     *
     * @param attachment  Which attachment.
     * @return
     */
    GLTexture *attachedTexture(Flags const &attachment) const;

    /**
     * Replaces a currently attached texture with another.
     *
     * @param attachment  Which attachment.
     * @param texture     New texture to use as the attachment.
     */
    void replaceAttachment(Flags const &attachment, GLTexture &texture);

    /**
     * Sets the target that is actually bound when this GLTarget is bound.
     * Intended to be used with multisampled buffers.
     *
     * @param proxy  Proxy target.
     */
    void setProxy(GLTarget const *proxy);

    void updateFromProxy();

    /**
     * Blits this target's contents to the @a copy target.
     *
     * @param dest         Target where to copy this target's contents.
     * @param attachments  Which attachments to blit.
     * @param filtering    Filtering to use if source and destinations sizes
     *                     aren't the same.
     */
    void blit(GLTarget &dest, Flags const &attachments = Color, gl::Filter filtering = gl::Nearest) const;

    /**
     * Sets the subregion inside the render target where scissor and viewport
     * will be scaled into. Scissor and viewport can still be defined as if the
     * entire window was in use; this only applies an offset and scaling to
     * both.
     *
     * @param rect   Target window rectangle. Set a null rectangle to
     *               use the entire window (like normally).
     * @param apply  Immediately update current OpenGL state accordingly.
     */
    void setActiveRect(Rectangleui const &rect, bool applyGLState = false);

    void unsetActiveRect(bool applyGLState = false);

    Vector2f activeRectScale() const;
    Vector2f activeRectNormalizedOffset() const;
    Rectangleui scaleToActiveRect(Rectangleui const &rect) const;
    Rectangleui const &activeRect() const;
    bool hasActiveRect() const;

    /**
     * Returns the area of the target currently in use. This is the full area
     * of the target, if there is no active rectangle specified. Otherwise
     * some sub-area of the target is returned.
     */
    Rectangleui rectInUse() const;

private:
    DENG2_PRIVATE(d)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(GLTarget::Flags)

} // namespace de

#endif // LIBGUI_GLTARGET_H
