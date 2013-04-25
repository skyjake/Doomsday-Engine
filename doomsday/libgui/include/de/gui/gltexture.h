/** @file gltexture.h  GL texture.
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

#ifndef LIBGUI_GLTEXTURE_H
#define LIBGUI_GLTEXTURE_H

#include <de/libdeng2.h>
#include <de/Asset>
#include <de/Vector>

#include "libgui.h"
#include "opengl.h"
#include "../Image"

namespace de {

namespace gl {
    enum Filter {
        Nearest,
        Linear
    };
    enum MipFilter {
        MipNone,
        MipNearest,
        MipLinear
    };
    enum Wrapping {
        Repeat,
        RepeatMirrored,
        ClampToEdge
    };
    enum CubeFace {
        PositiveX,
        NegativeX,
        PositiveY,
        NegativeY,
        PositiveZ,
        NegativeZ
    };
}

/**
 * GL texture object.
 *
 * Supports cube maps (6 faces/images instead of one). A GLTexture becomes a
 * cube map automatically when one sets image content to one of the faces.
 * Similarly a GLTexture reverts back to a 2D texture when setting non-cubeface
 * image content.
 *
 * @ingroup gl
 */
class LIBGUI_PUBLIC GLTexture : public Asset
{
public:
    typedef Vector2ui Size;
    typedef Vector2<gl::Wrapping> Wraps;

public:
    /**
     * Constructs a texture without any content. GLTexture instances are not
     * ready until they have some content defined.
     */
    GLTexture();

    /**
     * Release all image content associated with the texture.
     */
    void clear();

    void setMagFilter(gl::Filter magFilter);
    void setMinFilter(gl::Filter minFilter, gl::MipFilter mipFilter);
    void setFilter(gl::Filter magFilter, gl::Filter minFilter, gl::MipFilter mipFilter) {
        setMagFilter(magFilter);
        setMinFilter(minFilter, mipFilter);
    }
    void setWrapS(gl::Wrapping mode);
    void setWrapT(gl::Wrapping mode);
    inline void setWrap(gl::Wrapping s, gl::Wrapping t) {
        setWrapS(s);
        setWrapT(t);
    }
    inline void setWrap(Wraps const &st) {
        setWrapS(st.x);
        setWrapT(st.y);
    }

    gl::Filter minFilter() const;
    gl::Filter magFilter() const;
    gl::MipFilter mipFilter() const;
    gl::Wrapping wrapS() const;
    gl::Wrapping wrapT() const;
    Wraps wrap() const;

    bool isCubeMap() const;

    /**
     * Enables or disables automatic mipmap generation on the texture.
     *
     * @param genMips  @c true to generate mipmaps whenever level 0 changes.
     */
    void setAutoGenMips(bool genMips);

    bool autoGenMips() const;

    /**
     * Reserves a specific size of undefined memory for a level.
     *
     * @param size    Size in texels.
     * @param format  Pixel format that will be later used for uploading content.
     *                Determines internal storage pixel format.
     * @param level   Mipmap level.
     */
    void setUndefinedImage(Size const &size, Image::Format format, int level = 0);

    /**
     * Reserves a specific size of undefined memory for a cube map level.
     *
     * @param face   Face of a cube map.
     * @param size   Size in texels.
     * @param level  Mipmap level.
     */
    void setUndefinedImage(gl::CubeFace face, Size const &size, Image::Format format, int level = 0);

    /**
     * Sets the image content of the texture at a particular level. The format
     * of the image determines which GL format is chosen for the texture.
     *
     * @param image  Image to upload to a GL texture.
     * @param level  Level on which to store the image.
     */
    void setImage(Image const &image, int level = 0);

    /**
     * Sets the image content of the texture at a particular level. The format
     * of the image determines which GL format is chosen for the texture.
     *
     * @param face   Face of a cube map.
     * @param image  Image to upload to a GL texture.
     * @param level  Level on which to store the image.
     */
    void setImage(gl::CubeFace face, Image const &image, int level = 0);

    /**
     * Replaces a portion of existing content. The image should be provided in
     * the same format as the previous full content.
     *
     * @param image  Image to copy.
     * @param pos    Position where the image is being copied.
     * @param level  Mipmap level.
     */
    void setSubImage(Image const &image, Vector2i const &pos, int level = 0);

    void setSubImage(gl::CubeFace face, Image const &image, Vector2i const &pos, int level = 0);

    /**
     * Generate a full set of mipmap levels based on the content on level 0.
     * Automatic mipmap generation can be enabled with setAutoGenMips().
     */
    void generateMipmap();

    /**
     * Returns the size of the texture (mipmap level 0).
     */
    Size size() const;

    /**
     * Returns the number of mipmap levels in use by the texture.
     *
     * Use levelsForSize() to determine the number of mipmap levels for an
     * arbitrary texture size.
     */
    int mipLevels() const;

    /**
     * Returns the size of a particular mipmap level. This can be called after
     * the level 0 image content has been defined.
     *
     * @param level  Mip level.
     * @return Size in texels. (0,0) for an invalid level.
     */
    Size levelSize(int level) const;

    GLuint glName() const;

    void glBindToUnit(int unit) const;

public:
    /**
     * Calculates how many mipmap levels are produced for a specific size
     * of mipmap level 0 content.
     *
     * @param size  Size in texels at level 0.
     *
     * @return Number of mipmap levels.
     */
    static int levelsForSize(Size const &size);

    static Size levelSize(Size const &size0, int level);

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_GLTEXTURE_H
