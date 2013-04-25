/** @file image.h  Wrapper over QImage.
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

#ifndef LIBGUI_IMAGE_H
#define LIBGUI_IMAGE_H

#include <utility>
#include <QImage>
#include <de/Vector>
#include <de/Rectangle>
#include <de/ByteRefArray>

#include "libgui.h"

namespace de {

/**
 * Thin wrapper over QImage allowing use of some custom/raw image formats.
 *
 * Some image formats do not allow drawing. As a rule, all QImage-based formats
 * support drawing (via QPainter).
 *
 * @todo Merge image_s and the related Image_* routines into here.
 */
class LIBGUI_PUBLIC Image
{
public:
    /**
     * Supported GL-friendly formats. Note that all QImage formats cannot be
     * uploaded to OpenGL.
     */
    enum Format
    {
        UseQImageFormat,    ///< May not be GL friendly.

        Luminance_8,
        LuminanceAlpha_88,
        Alpha_8,
        RGB_555,
        RGB_565,
        RGB_444,
        RGB_888,            ///< 24-bit depth.
        RGBA_4444,
        RGBA_5551,
        RGBA_8888,
        RGBx_8888           ///< 32-bit depth, alpha data ignored.
    };

    typedef Vector2ui Size;
    typedef Vector4ub Color;

    /// GL format + type for glTex(Sub)Image.
    struct GLFormat {
        duint format;
        duint type;
        duint rowAlignment;

        GLFormat(duint f, duint t, duint ra) : format(f), type(t), rowAlignment(ra) {}
    };

public:
    Image();
    Image(Image const &other);
    Image(QImage const &image);

    /**
     * Constructs an image, taking a copy of the pixel data.
     *
     * @param size    Size of the image.
     * @param format  Data format.
     * @param pixels  Pixel data.
     */
    Image(Size const &size, Format format, IByteArray const &pixels);

    Image(Size const &size, Format format, ByteRefArray const &refPixels);

    Image &operator = (Image const &other);
    Image &operator = (QImage const &other);

    Format format() const;
    QImage::Format qtFormat() const;

    Size size() const;
    Rectanglei rect() const;

    /**
     * Number of bits per pixel.
     */
    int depth() const;

    /**
     * Number of bytes between rows in the pixel data.
     */
    int stride() const;

    /**
     * Total number of bytes in the pixel data.
     */
    int byteCount() const;

    void const *bits() const;

    void *bits();

    /**
     * Determines if the image format can be uploaded to OpenGL without
     * conversion of any kind.
     */
    bool isGLCompatible() const;

    bool canConvertToQImage() const;

    /**
     * Converts the image to a QImage. Only allowed if canConvertToQImage()
     * returns @c true.
     *
     * @return QImage instance.
     */
    QImage toQImage() const;

    GLFormat glFormat() const;

    // Drawing/editing methods.
    Image subImage(Rectanglei const &subArea) const;
    void resize(Size const &size);
    void fill(Color const &color);
    void fill(Rectanglei const &rect, Color const &color);
    void draw(Image const &image, Vector2i const &topLeft);

public:
    static GLFormat glFormat(Format imageFormat);
    static GLFormat glFormat(QImage::Format qtImageFormat);

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_IMAGE_H
