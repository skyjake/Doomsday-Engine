/** @file glframebuffer.h  GL frame buffer.
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

#ifndef LIBGUI_GLTEXTUREFRAMEBUFFER_H
#define LIBGUI_GLTEXTUREFRAMEBUFFER_H

#include <de/vector.h>
#include <de/asset.h>

#include "de/glframebuffer.h"
#include "de/gltexture.h"
#include "de/image.h"

namespace de {

/*namespace gl {
    enum SwapBufferMode {
        SwapMonoBuffer,
        SwapStereoLeftBuffer,
        SwapStereoRightBuffer,
        SwapStereoBuffers,
        SwapWithAlpha
    };
}*/

/**
 * OpenGL framebuffer object that stores color, depth, and stencil values in GL textures.
 * Automatically sets up and updates a render target where the textures are attached.
 * Provides a way to swap the contents of the framebuffer to a Canvas's back buffer.
 *
 * The framebuffer must be manually resized as appropriate.
 *
 * @ingroup gl
 */
class LIBGUI_PUBLIC GLTextureFramebuffer : public GLFramebuffer
{
public:
    GLTextureFramebuffer(Image::Format colorFormat = Image::RGB_888,
                         const Size &  initialSize = Size(),
                         int           sampleCount = 0 /*default*/);

    using Formats = List<Image::Format>;

    GLTextureFramebuffer(Formats colorFormats);

    bool areTexturesReady() const;

    void glInit();
    void glDeinit();

    void setSampleCount(int sampleCount);
    void setColorFormat(Image::Format colorFormat);

    /**
     * Resizes the framebuffer's textures. If the new size is the same as the
     * current one, nothing happens.
     *
     * @param newSize  New size for the framebuffer.
     */
    void resize(const Size &newSize);

    /**
     * Updates the color and depth textures by blitting from the multisampled
     * renderbuffers.
     */
    void resolveSamples();

    GLFramebuffer &resolvedFramebuffer();

    Size size() const;
    GLTexture &colorTexture() const;
    GLTexture &depthStencilTexture() const;
    int sampleCount() const;

    GLTexture *attachedTexture(Flags attachment) const override;

public:
    /**
     * Sets the default sample count for all frame buffers.
     *
     * All existing GLTextureFramebuffer instances that have been set to use the default sample
     * count will be updated to use the new count (i.e., contents lost).
     *
     * @param sampleCount  Sample count.
     *
     * @return @c true, iff the default value was changed.
     */
    static bool setDefaultMultisampling(int sampleCount);

    static int defaultMultisampling();

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_GLTEXTUREFRAMEBUFFER_H
