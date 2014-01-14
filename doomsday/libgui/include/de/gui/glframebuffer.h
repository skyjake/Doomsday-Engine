/** @file glframebuffer.h  GL frame buffer.
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

#ifndef LIBGUI_GLFRAMEBUFFER_H
#define LIBGUI_GLFRAMEBUFFER_H

#include <de/Vector>
#include <de/Asset>

#include "../GLTarget"
#include "../GLTexture"
#include "../Image"

namespace de {

namespace gl {
    enum SwapBufferMode {
        SwapMonoBuffer,
        SwapStereoLeftBuffer,
        SwapStereoRightBuffer,
        SwapStereoBuffers
    };
}

class Canvas;

/**
 * GL framebuffer that stores color, depth, and stencil values in GL textures.
 * Automatically sets up and updates a render target where the textures are
 * attached. Provides a way to swap the contents of the framebuffer to a
 * Canvas's back buffer.
 *
 * The framebuffer must be manually resized as appropriate.
 *
 * Supports multisampling using EXT_framebuffer_multisample and
 * EXT_framebuffer_blit.
 */
class LIBGUI_PUBLIC GLFramebuffer : public Asset
{
public:
    typedef Vector2ui Size;

public:
    GLFramebuffer(Image::Format const & colorFormat = Image::RGB_888,
                  Size const &          initialSize = Size(),
                  int                   sampleCount = 0 /*default*/);

    void glInit();
    void glDeinit();

    void setSampleCount(int sampleCount);
    void setColorFormat(Image::Format const &colorFormat);

    /**
     * Resizes the framebuffer's textures. If the new size is the same as the
     * current one, nothing happens.
     *
     * @param newSize  New size for the framebuffer.
     */
    void resize(Size const &newSize);

    Size size() const;
    GLTarget &target() const;
    GLTexture &colorTexture() const;
    GLTexture &depthStencilTexture() const;

    /**
     * Swaps buffers.
     *
     * @param canvas    Canvas where to swap.
     * @param swapMode  Stereo swapping mode:
     *      - gl::SwapMonoBuffer: swap is done normally into the Canvas's framebuffer
     *      - gl::SwapStereoLeftBuffer: swap updates the back/left stereo buffer
     *      - gl::SwapStereoRightBuffer: swap updates the back/right stereo buffer
     */
    void swapBuffers(Canvas &canvas, gl::SwapBufferMode swapMode = gl::SwapMonoBuffer);

public:
    /**
     * Sets the default sample count for all frame buffers.
     *
     * All existing GLFramebuffer instances that have been set to use the default sample
     * count will be updated to use the new count (i.e., contents lost).
     *
     * @param sampleCount  Sample count.
     *
     * @return @c true, iff the default value was changed.
     */
    static bool setDefaultMultisampling(int sampleCount);

    static int defaultMultisampling();

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_GLFRAMEBUFFER_H
