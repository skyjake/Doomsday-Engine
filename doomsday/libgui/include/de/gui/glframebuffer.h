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

class Canvas;

/**
 * GL framebuffer.
 *
 * Color values and depth/stencil values are written to textures.
 */
class GLFramebuffer : public Asset
{
public:
    typedef Vector2ui Size;

public:
    GLFramebuffer(Image::Format const & colorFormat = Image::RGB_888,
                  Size const &          initialSize = Size());

    void glInit();
    void glDeinit();

    void setColorFormat(Image::Format const &colorFormat);
    void resize(Size const &newSize);

    Size size() const;
    GLTarget &target() const;
    GLTexture &colorTexture() const;
    GLTexture &depthStencilTexture() const;

    void swapBuffers(Canvas &canvas);

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_GLFRAMEBUFFER_H
