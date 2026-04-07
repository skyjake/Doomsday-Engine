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

#include <de/vector.h>
#include <de/rectangle.h>
#include <de/byterefarray.h>
#include <de/iserializable.h>

#include "libgui.h"
#include "de/glpixelformat.h"

#include <utility>

namespace de {

class File;
class NativePath;

/**
 * Reading, writing, and modifying pixel-based images.
 *
 * @ingroup gl
 */
class LIBGUI_PUBLIC Image : public ISerializable, public Deletable
{
public:
    /**
     * Supported GL-friendly formats. Note that all QImage formats cannot be
     * uploaded to OpenGL.
     */
    enum Format {
        Unknown           = -1,

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
        R_8               = 12,
        RG_88             = 13,
        R_16f             = 14,
        RG_16f            = 15,
        RGB_16f           = 16,
        RGBA_16f          = 17,
        R_32f             = 18,
        RG_32f            = 19,
        RGB_32f           = 20,
        RGBA_32f          = 21,
        R_32i             = 22,
        RG_32i            = 23,
        RGB_32i           = 24,
        RGBA_32i          = 25,
        R_32ui            = 26,
        RG_32ui           = 27,
        RGB_32ui          = 28,
        RGBA_32ui         = 29,
    };

    enum SerializationFormat { Png, Jpeg, Targa, Bmp };

    typedef Vec2ui Size;
    typedef Vec4ub Color;
    typedef Vector4<duint16> Color16;

    static inline Color makeColor(int r, int g, int b, int a = 255)
    {
        return Color(duint8(clamp(0, r, 255)),
                     duint8(clamp(0, g, 255)),
                     duint8(clamp(0, b, 255)),
                     duint8(clamp(0, a, 255)));
    }
    static inline duint32 packColor(Color color)
    {
        return color.x | (color.y << 8) | (color.z << 16) | (color.w << 24);
    }
    static inline duint32 packColor(Color16 color)
    {
        return (color.x & 0xff) | ((color.y & 0xff) << 8) | ((color.z & 0xff) << 16) |
               ((color.w & 0xff) << 24);
    }
    static inline Color unpackColor(uint32_t packed)
    {
        return Color( packed        & 0xff,
                     (packed >> 8)  & 0xff,
                     (packed >> 16) & 0xff,
                     (packed >> 24) & 0xff);
    }
    static inline Color16 unpackColor16(uint32_t packed)
    {
        return Color16( packed        & 0xff,
                       (packed >> 8)  & 0xff,
                       (packed >> 16) & 0xff,
                       (packed >> 24) & 0xff);
    }
    static Vec4f hsv(Color color);          // normalized HSV
    static Color fromHsv(const Vec4f &hsv); // normalized HSV
    static Color mix(Color a, Color b, Color m);

public:
    Image();
    Image(const Image &);
    Image(Image &&);

    /**
     * Constructs an image with uninitialized contents.
     *
     * @param size    Size of the image.
     * @param format  Data format.
     */
    Image(const Size &size, Format format);

    /**
     * Constructs an image, taking a copy of the pixel data.
     *
     * @param size    Size of the image.
     * @param format  Data format.
     * @param pixels  Pixel data.
     */
    Image(const Size &size, Format format, const IByteArray &pixels);
    
    /**
     * Constructs an image.
     *
     * Note that Blocks use copy-on-write semantics, so only a reference to the data is held.
     *
     * @param size    Size of the image.
     * @param format  Data format.
     * @param pixels  Pixel data.
     */
    Image(const Size &size, Format format, const Block &pixels);

    /**
     * Constructs an image using pixels allocated and owned by someone else.
     * No copy is made of the pixel data.
     *
     * @param size       Size of the image.
     * @param format     Data format.
     * @param refPixels  Pixel data. No copy is made.
     */
    Image(const Size &size, Format format, const ByteRefArray &refPixels);

    Image &operator=(const Image &);
    Image &operator=(Image &&);

    inline explicit operator bool() const { return !isNull(); }

    Format format() const;

    Size size() const;
    Rectanglei rect() const;

    inline duint width() const { return size().x; }
    inline duint height() const { return size().y; }

    /**
     * Number of bits per pixel.
     */
    duint depth() const;

    inline duint bytesPerPixel() const { return depth() / 8; }

    /**
     * Number of bytes between rows in the pixel data.
     */
    dsize stride() const;

    /**
     * Total number of bytes in the pixel data.
     */
    dsize byteCount() const;

    const dbyte *bits() const;
    dbyte *      bits();

    const dbyte *row(duint y) const;
    dbyte *      row(duint y);
    dbyte *      rowEnd(duint y);

    const duint32 *row32(duint y) const;
    duint32 *      row32(duint y);
    duint32 *      rowEnd32(duint y);

    /**
     * Determines if the image has a zero size (no pixels).
     */
    bool isNull() const;

    /**
     * Determines if the image format can be uploaded to OpenGL without
     * conversion of any kind.
     */
    bool isGLCompatible() const;

    bool hasAlphaChannel() const;

//    bool canConvertToQImage() const;

    Image convertToFormat(Format format) const;

    /**
     * Converts the image to a QImage. Only allowed if canConvertToQImage()
     * returns @c true.
     *
     * @return QImage instance.
     */
//    QImage toQImage() const;

    GLPixelFormat glFormat() const;

    /**
     * Returns the ratio of how many points there are for each image pixel.
     * This can be used to scale the image appropriately for the UI.
     *
     * @return Points per image pixel.
     */
    float pointRatio() const;
    void setPointRatio(float pointsPerPixel);

    Vec2i origin() const;
    void setOrigin(const Vec2i &origin);

    inline Color pixel(duint x, duint y) const { return pixel(Vec2ui(x, y)); }
    Color pixel(Vec2ui pos) const;
    
    // Drawing/editing methods.
    Image       subImage(const Rectanglei &subArea) const;
    void        resize(const Size &size);
    void        fill(Color color);
    void        fill(const Rectanglei &rect, Color color);
    inline void setPixel(duint x, duint y, Color color) { setPixel(Vec2ui(x, y), color); }
    void        setPixel(Vec2ui pos, Color color);
    void        drawRect(const Rectanglei &rect, Color color);
    void        draw(const Image &image, const Vec2i &topLeft);
    inline void draw(int x, int y, const Image &image) { draw(image, Vec2i(x, y)); }
    void        drawPartial(const Image &image, const Rectanglei &part, const Vec2i &topLeft);
    inline void draw(int x, int y, const Image &image, int subX, int subY, int subW, int subH)
    {
        drawPartial(image, Rectanglei(subX, subY, subW, subH), Vec2i(x, y));
    }

    Image multiplied(const Image &factorImage) const;
    Image multiplied(Color color) const;
    Image colorized(Color color) const;
    Image invertedColor() const;
    Image mixed(const Image &low, const Image &high) const;
    Image mixed(const Color &zero, const Color &one, const int componentIndices[4] = nullptr) const;
    Image withAlpha(const Image &grayscale) const;
    Image rgbSwapped() const;
    Image flipped() const;

    void  save(const NativePath &path) const;
    Block serialize(SerializationFormat format) const;
    Block serialize(const char *formatHint) const;

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

public:
    static GLPixelFormat glFormat(Format imageFormat);
//    static GLPixelFormat glFormat(QImage::Format qtImageFormat);

    static Image solidColor(Color color, const Size &size);

    /**
     * Loads an image from a block of data. The format of the image is autodetected.
     * In addition to image formats supported by Qt, this can load 8-bit paletted PCX
     * (ZSoft Paintbrush) images.
     *
     * @param data        Block of data containing image data.
     * @param formatHint  Optionally, file name extension with dot included (".tga").
     */
    static Image fromData(const IByteArray &data, const String &formatHint = "");

    static Image fromXpmData(const char * const *xpmStrings);

    /// @copydoc fromData()
    static Image fromData(const Block &data, const String &formatHint = "");

    static Image fromRgbaData(const Size &size, const IByteArray &rgba);

    static Image fromIndexedData(const Size &      size,
                                 const IByteArray &image,
                                 const IByteArray &palette);

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
    static Image fromMaskedIndexedData(const Image::Size &size,
                                       const IByteArray &imageAndMask,
                                       const IByteArray &palette);

    /**
     * Attempts to recognize if a file contains a supported image content format.
     * @param file  File whose contents to recognize.
     * @return `true` if image data can be loaded from the file.
     */
    static bool recognize(const File &file);

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_IMAGE_H
