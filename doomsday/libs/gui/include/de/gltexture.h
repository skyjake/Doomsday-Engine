/** @file gltexture.h  GL texture.
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

#ifndef LIBGUI_GLTEXTURE_H
#define LIBGUI_GLTEXTURE_H

#include <de/libcore.h>
#include <de/asset.h>
#include <de/vector.h>

#include "libgui.h"
#include "opengl.h"
#include "de/image.h"
#include "de/glpixelformat.h"
#include "de/glstate.h"

namespace de {

namespace gfx {
    enum Filter {
        Nearest,
        Linear,
    };
    enum MipFilter {
        MipNone,
        MipNearest,
        MipLinear,
    };
    enum Wrapping {
        Repeat,
        RepeatMirrored,
        ClampToEdge,
        ClampToBorder,
    };
    enum CubeFace {
        PositiveX,
        NegativeX,
        PositiveY,
        NegativeY,
        PositiveZ,
        NegativeZ,
    };
    enum ComparisonMode {
        CompareNone,
        CompareRefToTexture,
    };
} // namespace gfx

/**
 * GL texture object.
 *
 * Supports cube maps (6 faces/images instead of one). A GLTexture becomes a
 * cube map automatically when one sets image content to one of the faces.
 * Similarly a GLTexture reverts back to a 2D texture when setting non-cubeface
 * image content.
 *
 * Mipmaps can be generated automatically (see GLTexture::generateMipmap() and
 * GLTexture::setAutoGenMips()). By default, mipmaps are not generated
 * automatically.
 *
 * @ingroup gl
 */
class LIBGUI_PUBLIC GLTexture : public Asset
{
public:
    using Size  = Vec2ui;
    using Wraps = Vector2<gfx::Wrapping>;

public:
    /**
     * Constructs a texture without any content. GLTexture instances are not
     * ready until they have some content defined.
     */
    GLTexture();

    /**
     * Constructs a texture from an existing OpenGL texture object. Ownership
     * of the texture is transferred to the new GLTexture instance.
     *
     * @param existingTexture  Existing texture.
     * @param size             Size of the texture in texels.
     */
    GLTexture(GLuint existingTexture, const Size &size);

    /**
     * Release all image content associated with the texture.
     */
    void clear();

    void setMagFilter(gfx::Filter magFilter);
    void setMinFilter(gfx::Filter minFilter, gfx::MipFilter mipFilter);
    void setFilter(gfx::Filter magFilter, gfx::Filter minFilter, gfx::MipFilter mipFilter) {
        setMagFilter(magFilter);
        setMinFilter(minFilter, mipFilter);
    }
    void setWrapS(gfx::Wrapping mode);
    void setWrapT(gfx::Wrapping mode);
    inline void setWrap(gfx::Wrapping s, gfx::Wrapping t) {
        setWrapS(s);
        setWrapT(t);
    }
    inline void setWrap(const Wraps &st) {
        setWrapS(st.x);
        setWrapT(st.y);
    }
    void setMaxAnisotropy(dfloat maxAnisotropy);
    void setMaxLevel(dfloat maxLevel);
    void setBorderColor(const Vec4f &color);
    void setComparisonMode(gfx::ComparisonMode mode, gfx::Comparison func = gfx::Always);

    gfx::Filter    minFilter() const;
    gfx::Filter    magFilter() const;
    gfx::MipFilter mipFilter() const;
    gfx::Wrapping  wrapS() const;
    gfx::Wrapping  wrapT() const;
    Wraps          wrap() const;
    dfloat         maxAnisotropy() const;
    dfloat         maxLevel() const;
    de::Vec4f      borderColor() const;

    bool isCubeMap() const;

    /**
     * Enables or disables automatic mipmap generation on the texture. By
     * default, automatic mipmap generation is disabled.
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
    void setUndefinedImage(const Size &size, Image::Format format, int level = 0);

    /**
     * Reserves a specific size of undefined memory for a cube map level.
     *
     * @param face    Face of a cube map.
     * @param size    Size in texels.
     * @param format  Pixel format that will be later used for uploading content.
     *                Determines internal storage pixel format.
     * @param level   Mipmap level.
     */
    void setUndefinedImage(gfx::CubeFace face, const Size &size, Image::Format format, int level = 0);

    void setUndefinedContent(const Size &size, const GLPixelFormat &glFormat, int level = 0);

    void setUndefinedContent(gfx::CubeFace face, const Size &size, const GLPixelFormat &glFormat, int level = 0);

    void setDepthStencilContent(const Size &size);

    /**
     * Sets the image content of the texture at a particular level. The format
     * of the image determines which GL format is chosen for the texture.
     *
     * @param image  Image to upload to a GL texture.
     * @param level  Level on which to store the image.
     */
    void setImage(const Image &image, int level = 0);

    /**
     * Sets the image content of the texture at a particular level. The format
     * of the image determines which GL format is chosen for the texture.
     *
     * @param face   Face of a cube map.
     * @param image  Image to upload to a GL texture.
     * @param level  Level on which to store the image.
     */
    void setImage(gfx::CubeFace face, const Image &image, int level = 0);

    /**
     * Replaces a portion of existing content. The image must be provided in
     * the same format as the previous full content.
     *
     * @param image  Image to copy.
     * @param pos    Position where the image is being copied.
     * @param level  Mipmap level.
     */
    void setSubImage(const Image &image, const Vec2i &pos, int level = 0);

    /**
     * Replaces a portion of existing content. The image must be provided in the same
     * format as the previous full content.
     *
     * The image data is copied directly from the specified subregion of the image.
     *
     * @param image  Image to copy.
     * @param rect   Region of the image to copy.
     * @param level  Mipmap level.
     */
    void setSubImage(const Image &image, const Rectanglei &rect, int level = 0);

    void setSubImage(gfx::CubeFace face, const Image &image, const Vec2i &pos, int level = 0);
    void setSubImage(gfx::CubeFace face, const Image &image, const Rectanglei &rect, int level = 0);

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

    void glBindToUnit(duint unit) const;

    /**
     * Applies any cached parameter changes to the GL texture object. The texture is
     * bound temporarily while this is done (so any previously bound texture is unbound).
     */
    void glApplyParameters();

    /**
     * Returns the image format that was specified when image content was put
     * into the texture (with setImage() or setSubImage()).
     */
    Image::Format imageFormat() const;

public:
    /**
     * Determines the maximum supported texture size.
     */
    static Size maximumSize();

protected:
    /**
     * Derived classes can override this to perform additional tasks
     * immediately before the texture is bound for use. The default
     * implementation does not need to be called.
     */
    virtual void aboutToUse() const;

public:
    /**
     * Calculates how many mipmap levels are produced for a specific size
     * of mipmap level 0 content.
     *
     * @param size  Size in texels at level 0.
     *
     * @return Number of mipmap levels.
     */
    static int levelsForSize(const Size &size);

    static Size levelSize(const Size &size0, int level);

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_GLTEXTURE_H
