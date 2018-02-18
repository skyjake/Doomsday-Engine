/** @file image.h  Wrapper over QImage.
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

class File;

/**
 * Thin wrapper over QImage allowing use of some custom/raw image formats.
 *
 * Some image formats do not allow drawing. As a rule, all QImage-based formats
 * support drawing (via QPainter).
 *
 * @todo Merge image_t and the related Image_* routines into here.
 *
 * @ingroup gl
 */
class LIBGUI_PUBLIC Image : public ISerializable
{
public:
    /**
     * Supported GL-friendly formats. Note that all QImage formats cannot be
     * uploaded to OpenGL.
     */
    enum Format {
        Unknown           = -1,
        UseQImageFormat   = 0, ///< May not be GL friendly.

        Luminance_8       = 1,
        LuminanceAlpha_88 = 2,
        Alpha_8           = 3,
        RGB_555           = 4,
        RGB_565           = 5,
        RGB_444           = 6,
        RGB_888           = 7, ///< 24-bit depth.
        RGBA_4444         = 8,
        RGBA_5551         = 9,
        RGBA_8888         = 10,
        RGBx_8888         = 11, ///< 32-bit depth, alpha data ignored.
        R_16f             = 12,
        RG_16f            = 13,
        RGB_16f           = 14,
        RGBA_16f          = 15,
        R_32f             = 16,
        RG_32f            = 17,
        RGB_32f           = 18,
        RGBA_32f          = 19,
        R_32i             = 20,
        RG_32i            = 21,
        RGB_32i           = 22,
        RGBA_32i          = 23,
        R_32ui            = 24,
        RG_32ui           = 25,
        RGB_32ui          = 26,
        RGBA_32ui         = 27,
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

    inline explicit operator bool() const { return !isNull(); }

    Format format() const;
    QImage::Format qtFormat() const;

    Size size() const;
    Rectanglei rect() const;

    inline duint width() const { return size().x; }
    inline duint height() const { return size().y; }

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

    /**
     * Returns the ratio of how many points there are for each image pixel.
     * This can be used to scale the image appropriately for the UI.
     *
     * @return Points per image pixel.
     */
    float pointRatio() const;

    void setPointRatio(float pointsPerPixel);

    // Drawing/editing methods.
    Image subImage(Rectanglei const &subArea) const;
    void resize(Size const &size);
    void fill(Color const &color);
    void fill(Rectanglei const &rect, Color const &color);
    void draw(Image const &image, Vector2i const &topLeft);
    void drawPartial(Image const &image, Rectanglei const &part, Vector2i const &topLeft);
    Image multiplied(Image const &factorImage) const;
    Image multiplied(Color const &color) const;
    Image colorized(Color const &color) const;

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
     * @param data        Block of data containing image data.
     * @param formatHint  Optionally, file name extension with dot included (".tga").
     */
    static Image fromData(IByteArray const &data, String const &formatHint = "");

    /// @copydoc fromData()
    static Image fromData(Block const &data, String const &formatHint = "");

    static Image fromIndexedData(Image::Size const &size, IByteArray const &image,
                                 IByteArray const &palette);

    /**
     * Converts a color indexed 8-bit image to RGBA_8888.
     *
     * @param size       Dimensions of the image.
     * @param imageAndMask  Image content. This is w*h 8-bit palette indices followed
     *                   by w*h 8-bit alpha components, i.e., the alpha mask is
     *                   stored as a separate layer following the indices.
     * @param palette    RGB palette containing 8-bit color triplets. The size of the
     *                   palette must be big enough to contain all the color indices
     *                   used in the image data.
     *
     * @return Image in RGBA_8888 format.
     */
    static Image fromMaskedIndexedData(Image::Size const &size,
                                       IByteArray const &imageAndMask,
                                       IByteArray const &palette);

    /**
     * Attempts to recognize if a file contains a supported image content format.
     * @param file  File whose contents to recognize.
     * @return `true` if image data can be loaded from the file.
     */
    static bool recognize(File const &file);

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_IMAGE_H
