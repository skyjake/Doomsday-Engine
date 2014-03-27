/** @file image.h  Wrapper over QImage.
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

#ifndef LIBGUI_IMAGE_H
#define LIBGUI_IMAGE_H

#include <utility>
#include <QImage>
#include <de/Vector>
#include <de/Rectangle>
#include <de/ByteRefArray>
#include <de/ISerializable>

#include "../gui/libgui.h"
#include "../GLPixelFormat"

namespace de {

/**
 * Thin wrapper over QImage allowing use of some custom/raw image formats.
 *
 * Some image formats do not allow drawing. As a rule, all QImage-based formats
 * support drawing (via QPainter).
 *
 * @todo Merge image_t and the related Image_* routines into here.
 */
class LIBGUI_PUBLIC Image : public ISerializable
{
public:
    /**
     * Supported GL-friendly formats. Note that all QImage formats cannot be
     * uploaded to OpenGL.
     */
    enum Format
    {
        Unknown = -1,
        UseQImageFormat = 0,    ///< May not be GL friendly.

        Luminance_8 = 1,
        LuminanceAlpha_88 = 2,
        Alpha_8 = 3,
        RGB_555 = 4,
        RGB_565 = 5,
        RGB_444 = 6,
        RGB_888 = 7,            ///< 24-bit depth.
        RGBA_4444 = 8,
        RGBA_5551 = 9,
        RGBA_8888 = 10,
        RGBx_8888 = 11          ///< 32-bit depth, alpha data ignored.
    };

    typedef Vector2ui Size;
    typedef Vector4ub Color;

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

    duint width() const { return size().x; }
    duint height() const { return size().y; }

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
     * Determines if the image has a zero size (no pixels).
     */
    bool isNull() const;

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

    GLPixelFormat glFormat() const;

    // Drawing/editing methods.
    Image subImage(Rectanglei const &subArea) const;
    void resize(Size const &size);
    void fill(Color const &color);
    void fill(Rectanglei const &rect, Color const &color);
    void draw(Image const &image, Vector2i const &topLeft);
    void drawPartial(Image const &image, Rectanglei const &part, Vector2i const &topLeft);

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

public:
    static GLPixelFormat glFormat(Format imageFormat);
    static GLPixelFormat glFormat(QImage::Format qtImageFormat);

    static Image solidColor(Color const &color, Size const &size);

    /**
     * Loads an image from a block of data. The format of the image is autodetected.
     * In addition to image formats supported by Qt, this can load 8-bit paletted PCX
     * (ZSoft Paintbrush) images.
     *
     * @param data  Block of data containing image data.
     */
    static Image fromData(IByteArray const &data);

    /// @copydoc fromData()
    static Image fromData(Block const &data);

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_IMAGE_H
