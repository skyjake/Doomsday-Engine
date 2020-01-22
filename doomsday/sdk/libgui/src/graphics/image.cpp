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

#include "de/Image"
#include "de/graphics/opengl.h"

#include <de/Block>
#include <de/File>
#include <de/FixedByteArray>
#include <de/Log>
#include <de/Reader>
#include <de/Vector>
#include <de/Writer>
#include <de/Zeroed>

#include <QDataStream>
#include <QPainter>
#include <QColor>

namespace de {

#define IMAGE_ASSERT_EDITABLE(d) DENG2_ASSERT(d->format == UseQImageFormat)

namespace internal {

namespace pcx {

static dbyte const MAGIC        = 0x0a;
static dbyte const RLE_ENCODING = 1;
static dsize const HEADER_SIZE  = 128;

struct Header : public IReadable
{
    dbyte   magic;
    dbyte   version;
    dbyte   encoding;
    dbyte   bitsPerPixel;
    duint16 xMin, yMin;
    duint16 xMax, yMax;
    duint16 hRes, vRes;
    dbyte   colorPlanes;
    duint16 bytesPerLine;
    duint16 paletteType;

    void operator << (Reader &from)
    {
        from >> magic
             >> version
             >> encoding
             >> bitsPerPixel
             >> xMin
             >> yMin
             >> xMax
             >> yMax
             >> hRes
             >> vRes;

        from.seek(48);  // skip EGA palette
        from.seek(1);   // skip reserved field

        from >> colorPlanes
             >> bytesPerLine
             >> paletteType;
    }
};

static bool recognize(Block const &data)
{
    try
    {
        Header header;
        Reader(data) >> header;

        // Only paletted format supported.
        return (header.magic == MAGIC &&
                header.version == 5 /* latest */ &&
                header.encoding == RLE_ENCODING &&
                header.bitsPerPixel == 8);
    }
    catch (Error const &)
    {
        return false;
    }
}

/**
 * Loads a PCX image into a QImage using an RGB888 buffer. The PCX palette is used
 * to map color indices to RGB values.
 *
 * @param data  Source data containing a PCX image.
 *
 * @return QImage using the RGB888 (24-bit) format.
 */
static QImage load(Block const &data)
{
    Header header;
    Reader reader(data);
    reader >> header;

    Image::Size const size(header.xMax + 1, header.yMax + 1);

    QImage image(size.x, size.y, QImage::Format_RGB888);
    DENG2_ASSERT(image.depth() == 24);

    dbyte const *palette = data.data() + data.size() - 768;
    dbyte const *pos = data.data() + HEADER_SIZE;
    dbyte *dst = image.bits();

    for (duint y = 0; y < size.y; ++y, dst += size.x * 3)
    {
        for (duint x = 0; x < size.x; )
        {
            dbyte value = *pos++;

            // RLE inflation.
            int runLength = 1;
            if ((value & 0xc0) == 0xc0)
            {
                runLength = value & 0x3f;
                value = *pos++;
            }
            while (runLength-- > 0)
            {
                // Get the RGB triplets from the palette.
                std::memcpy(&dst[3 * x++], &palette[3 * value], 3);
            }
        }
    }

    return image;
}

} // namespace pcx

namespace tga {

/// @todo Replace this Targa loader with stb_image.

struct Header : public IReadable
{
    enum Flag
    {
        NoFlags           = 0,
        ScreenOriginUpper = 0x1,
        InterleaveTwoWay  = 0x2,
        InterleaveFourWay = 0x4
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    enum ColorMapType
    {
        ColorMapNone = 0,
        ColorMap256  = 1 // not supported
    };

    enum ImageType
    {
        ColorMapped = 1, // Uncompressed and color-mapped.
        RGB         = 2, // Uncompressed RGB.
        RleRGB      = 10 // Run-length encoded RGB.
    };

    Block identification;
    Zeroed<duint8> colorMapType;
    Zeroed<duint8> imageType;

    // Color map.
    Zeroed<dint16> mapIndex;
    Zeroed<dint16> mapCount;        ///< Number of color map entries.
    Zeroed<duint8> mapEntrySize;    ///< Bits in a color map entry.

    // Image specification.
    Flags flags;
    Vector2<dint16> origin;
    Vector2<dint16> size;
    Zeroed<duint8> depth;
    Zeroed<duint8> attrib;

    void operator << (Reader &from)
    {
        duint8 identificationSize;
        from >> identificationSize
             >> colorMapType
             >> imageType

             >> mapIndex
             >> mapCount
             >> mapEntrySize

             >> origin.x >> origin.y
             >> size.x >> size.y
             >> depth;

        duint8 f;
        from >> f;

        /*
           Flags:
             0-3 : Number of attribute bits
               4 : reserved
               5 : Screen origin in upper left corner
             6-7 : Data storage interleave
                   00 - no interleave
                   01 - even/odd interleave
                   10 - four way interleave
                   11 - reserved
        */

        attrib = f & 0x0f;

        flags = (f & 0x20? ScreenOriginUpper : NoFlags);
        if ((f & 0xc0) == 0x40) flags |= InterleaveTwoWay;
        if ((f & 0xc0) == 0x80) flags |= InterleaveFourWay;

        from.readBytes(identificationSize, identification);
    }
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Header::Flags)

static bool recognize(Block const &data)
{
    try
    {
        Header header;
        Reader(data) >> header;
        if (header.size.x == 0 || header.size.y == 0)
        {
            return false;
        }
        if (header.imageType == Header::ColorMapped && header.colorMapType == Header::ColorMap256 &&
            header.depth == 8)
        {
            return true;
        }
        return (header.imageType == Header::RGB || header.imageType == Header::RleRGB) &&
               header.colorMapType == Header::ColorMapNone &&
               (header.depth == 24 || header.depth == 32);
    }
    catch (...)
    {
        return false;
    }
}

static QImage load(Block const &data)
{
    Header header;
    Reader input(data);
    input >> header;

    int const pixelSize = header.depth / 8;
    QImage img(QSize(header.size.x, header.size.y),
               pixelSize == 4 || header.attrib > 0? QImage::Format_ARGB32 : QImage::Format_RGB888);
    dbyte *base = img.bits();

    bool const isUpperOrigin = header.flags.testFlag(Header::ScreenOriginUpper);

    // RGB can be read line by line.
    if (header.imageType == Header::RGB)
    {
        for (int y = 0; y < header.size.y; y++)
        {
            int inY = (isUpperOrigin? y : (header.size.y - y - 1));
            ByteRefArray line(base + (inY * img.bytesPerLine()), header.size.x * pixelSize);
            input.readBytesFixedSize(line);
        }
    }
    else if (header.imageType == Header::RleRGB)
    {
        img.fill(0);

        // RLE packets may cross over to the next line.
        int x = 0;
        int y = (isUpperOrigin? 0 : (header.size.y - 1));
        int endY = header.size.y - y - 1;
        int stepY = (isUpperOrigin? 1 : -1);

        while (y != endY && x < header.size.x)
        {
            dbyte rle;
            input >> rle;

            int count;
            bool repeat = false;
            if (rle & 0x80) // Repeat?
            {
                repeat = true;
                count = (rle & 0x7f) + 1;
            }
            else
            {
                count = rle + 1;
            }

            Block pixel;
            for (int i = 0; i < count; ++i)
            {
                if (i == 0 || !repeat)
                {
                    // Read the first/next byte.
                    input.readBytes(pixelSize, pixel);
                }

                std::memcpy(base + (x + y * header.size.x) * pixelSize,
                            pixel.constData(), pixelSize);

                // Advance the position.
                if (++x == header.size.x)
                {
                    x = 0;
                    y += stepY;
                }
            }
        }
    }
    else if (header.imageType == Header::ColorMapped)
    {
        DENG2_ASSERT(header.colorMapType == Header::ColorMap256);
        DENG2_ASSERT(header.depth == 8);

        // Read the colormap.
        QVector<QRgb> colorTable(256);
        for (int i = 0; i < header.mapCount; ++i)
        {
            uint8_t color[4] = {0, 0, 0, 255};
            ByteRefArray buf(color, 4);
            input.readBytes(header.mapEntrySize / 8, buf);

            DENG2_ASSERT(header.mapIndex + i < 256);

            // R/B swapped
            colorTable[header.mapIndex + i] = qRgba(color[2], color[1], color[0], color[3]);
        }

        img = QImage(QSize(header.size.x, header.size.y), QImage::Format_Indexed8);
        img.setColorTable(colorTable);

        dbyte *base = img.bits();
        for (int y = 0; y < header.size.y; y++)
        {
            int inY = (isUpperOrigin? y : (header.size.y - y - 1));
            ByteRefArray line(base + inY * img.bytesPerLine(), header.size.x);
            input.readBytesFixedSize(line);
        }
    }

    if (pixelSize >= 3)
    {
        img = img.rgbSwapped();
    }

    return img;
}

} // namespace tga

} // namespace internal
using namespace internal;

DENG2_PIMPL(Image)
{
    Format format;
    Size size;
    QImage image;
    Block pixels;
    ByteRefArray refPixels;
    float pointRatio = 1.f;

    Impl(Public *i, QImage const &img = QImage())
        : Base(i), format(UseQImageFormat), image(img)
    {
        size = Size(img.width(), img.height());
    }

    Impl(Public * i, Impl const &other)
        : Base(i)
        , format(other.format)
        , size(other.size)
        , image(other.image)
        , pixels(other.pixels)
        , refPixels(other.refPixels)
        , pointRatio(other.pointRatio)
    {}

    Impl(Public *i, Size const &imgSize, Format imgFormat, IByteArray const &imgPixels)
        : Base(i), format(imgFormat), size(imgSize), pixels(imgPixels)
    {}

    Impl(Public *i, Size const &imgSize, Format imgFormat, ByteRefArray const &imgRefPixels)
        : Base(i), format(imgFormat), size(imgSize), refPixels(imgRefPixels)
    {}
};

Image::Image() : d(new Impl(this))
{}

Image::Image(Image const &other)
    : d(new Impl(this, *other.d))
{}

Image::Image(QImage const &image) : d(new Impl(this, image))
{}

Image::Image(Size const &size, Format format, IByteArray const &pixels)
    : d(new Impl(this, size, format, pixels))
{}

Image::Image(Size const &size, Format format, ByteRefArray const &refPixels)
    : d(new Impl(this, size, format, refPixels))
{}

Image &Image::operator=(Image const &other)
{
    d.reset(new Impl(this, *other.d));
    return *this;
}

Image &Image::operator = (QImage const &other)
{
    d.reset(new Impl(this, other));
    return *this;
}

Image::Format Image::format() const
{
    return d->format;
}

QImage::Format Image::qtFormat() const
{
    if (d->format == UseQImageFormat)
    {
        return d->image.format();
    }
    return QImage::Format_Invalid;
}

Image::Size Image::size() const
{
    return d->size;
}

Rectanglei Image::rect() const
{
    return Rectanglei(0, 0, d->size.x, d->size.y);
}

int Image::depth() const
{
    switch (d->format)
    {
    case UseQImageFormat:
        return d->image.depth();

    case Luminance_8:
    case Alpha_8:
        return 8;

    case LuminanceAlpha_88:
    case RGB_565:
    case RGBA_4444:
    case RGBA_5551:
        return 16;

    case RGB_888:
        return 24;

    case RGBA_8888:
    case RGBx_8888:
        return 32;

    default:
        return 0;
    }
}

int Image::stride() const
{
    if (d->format == UseQImageFormat)
    {
        return d->image.bytesPerLine();
    }
    return depth() / 8 * d->size.x;
}

int Image::byteCount() const
{
    if (d->format == UseQImageFormat)
    {
        return d->image.byteCount();
    }
    if (!d->pixels.isEmpty())
    {
        return d->pixels.size();
    }
    return depth() / 8 * d->size.x * d->size.y;
}

void const *Image::bits() const
{
    if (d->format == UseQImageFormat)
    {
        return d->image.constBits();
    }
    if (!d->pixels.isEmpty())
    {
        return d->pixels.constData();
    }
    return d->refPixels.readBase();
}

void *Image::bits()
{
    if (d->format == UseQImageFormat)
    {
        return d->image.bits();
    }
    if (!d->pixels.isEmpty())
    {
        return d->pixels.data();
    }
    return d->refPixels.base();
}

bool Image::isNull() const
{
    return size() == Size(0, 0);
}

bool Image::isGLCompatible() const
{
    if (d->format == UseQImageFormat)
    {
        // Some QImage formats are GL compatible.
        switch (qtFormat())
        {
        case QImage::Format_ARGB32: // 8888
        case QImage::Format_RGB32:  // 8888
        case QImage::Format_RGB888: // 888
        case QImage::Format_RGB16:  // 565
        case QImage::Format_RGB555: // 555
        case QImage::Format_RGB444: // 444
            return true;

        default:
            return false;
        }
    }
    return d->format >= Luminance_8 && d->format <= RGBx_8888;
}

bool Image::canConvertToQImage() const
{
    switch (d->format)
    {
    case RGB_444:
    case RGB_555:
    case RGB_565:
    case RGB_888:
    case RGBA_8888:
    case RGBx_8888:
    case UseQImageFormat:
        return true;

    default:
        return false;
    }
}

QImage Image::toQImage() const
{
    if (d->format == UseQImageFormat)
    {
        return d->image;
    }

    // There may be some conversions we can do.
    QImage::Format form = QImage::Format_Invalid;
    switch (d->format)
    {
    case RGB_444:
        form = QImage::Format_RGB444;
        break;
    case RGB_555:
        form = QImage::Format_RGB555;
        break;
    case RGB_565:
        form = QImage::Format_RGB16;
        break;
    case RGB_888:
        form = QImage::Format_RGB888;
        break;
    case RGBA_8888:
        form = QImage::Format_ARGB32;
        break;
    case RGBx_8888:
        form = QImage::Format_RGB32;
        break;
    default:
        // Cannot be done.
        return QImage();
    }

    QImage img(QSize(d->size.x, d->size.y), form);
    std::memcpy(const_cast<uchar *>(img.constBits()), bits(), byteCount());
    return img;
}

GLPixelFormat Image::glFormat() const
{
    if (d->format == UseQImageFormat)
    {
        return glFormat(d->image.format());
    }
    return glFormat(d->format);
}

float Image::pointRatio() const
{
    return d->pointRatio;
}

void Image::setPointRatio(float pointsPerPixel)
{
    d->pointRatio = pointsPerPixel;
}

Image Image::subImage(Rectanglei const &subArea) const
{
    IMAGE_ASSERT_EDITABLE(d);

    return Image(d->image.copy(subArea.topLeft.x, subArea.topLeft.y,
                               subArea.width(), subArea.height()));
}

void Image::resize(Size const &size)
{
    IMAGE_ASSERT_EDITABLE(d);
    DENG2_ASSERT(d->image.format() != QImage::Format_Invalid);

    QImage resized(QSize(size.x, size.y), d->image.format());
    resized.fill(0);

    QPainter painter(&resized);
    painter.drawImage(QRect(QPoint(0, 0), resized.size()), d->image);
    d->image = resized;
    d->size = size;
}

void Image::fill(Color const &color)
{
    IMAGE_ASSERT_EDITABLE(d);

    d->image.fill(QColor(color.x, color.y, color.z, color.w).rgba());
}

void Image::fill(Rectanglei const &rect, Color const &color)
{
    IMAGE_ASSERT_EDITABLE(d);

    QPainter painter(&d->image);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(QRect(rect.topLeft.x, rect.topLeft.y, rect.width(), rect.height()),
                     QColor(color.x, color.y, color.z, color.w));
}

void Image::draw(Image const &image, Vector2i const &topLeft)
{
    IMAGE_ASSERT_EDITABLE(d);
    IMAGE_ASSERT_EDITABLE(image.d);

    QPainter painter(&d->image);
    painter.drawImage(QPoint(topLeft.x, topLeft.y), image.d->image);
}

void Image::drawPartial(Image const &image, Rectanglei const &part, Vector2i const &topLeft)
{
    IMAGE_ASSERT_EDITABLE(d);
    IMAGE_ASSERT_EDITABLE(image.d);

    QPainter painter(&d->image);
    painter.drawImage(QPoint(topLeft.x, topLeft.y),
                      image.d->image,
                      QRect(part.left(), part.top(), part.width(), part.height()));
}

Image Image::multiplied(Image const &factorImage) const
{
    QImage multiplied = toQImage();
    QPainter painter(&multiplied);
    painter.setCompositionMode(QPainter::CompositionMode_Multiply);
    painter.drawImage(0, 0, factorImage.toQImage());
    return multiplied;
}

Image Image::multiplied(Color const &color) const
{
    if (color == Color(255, 255, 255, 255)) return *this; // No change.

    QImage copy = toQImage().convertToFormat(QImage::Format_ARGB32);

    for (duint y = 0; y < height(); ++y)
    {
        duint32 *ptr = reinterpret_cast<duint32 *>(copy.bits() + y * copy.bytesPerLine());
        for (duint x = 0; x < width(); ++x)
        {
            duint16 b =  *ptr & 0xff;
            duint16 g = (*ptr & 0xff00) >> 8;
            duint16 r = (*ptr & 0xff0000) >> 16;
            duint16 a = (*ptr & 0xff000000) >> 24;

            *ptr++ = qRgba((color.x + 1) * r >> 8,
                           (color.y + 1) * g >> 8,
                           (color.z + 1) * b >> 8,
                           (color.w + 1) * a >> 8);
        }
    }
    return copy;
}

Image Image::colorized(Color const &color) const
{
    QImage copy = toQImage().convertToFormat(QImage::Format_ARGB32);

    QColor targetColor(color.x, color.y, color.z, 255);
    int targetHue = targetColor.hue();

    for (duint y = 0; y < height(); ++y)
    {
        duint32 *ptr = reinterpret_cast<duint32 *>(copy.bits() + y * copy.bytesPerLine());
        for (duint x = 0; x < width(); ++x)
        {
            duint16 b =  *ptr & 0xff;
            duint16 g = (*ptr & 0xff00) >> 8;
            duint16 r = (*ptr & 0xff0000) >> 16;
            duint16 a = (*ptr & 0xff000000) >> 24;

            QColor rgba(r, g, b, a);
            QColor colorized;
            colorized.setHsv(targetHue, rgba.saturation(), rgba.value(), color.w * a >> 8);

            *ptr++ = colorized.rgba();
        }
    }
    return copy;
}

void Image::operator >> (Writer &to) const
{
    to << duint8(d->format);

    if (d->format == UseQImageFormat)
    {
        Block block;
        QDataStream os(&block, QIODevice::WriteOnly);
        os.setVersion(QDataStream::Qt_4_8);
        os << d->image;
        to << block;
    }
    else
    {
        to << d->size << ByteRefArray(bits(), byteCount());
    }
}

void Image::operator << (Reader &from)
{
    d->pixels.clear();
    d->refPixels = ByteRefArray();

    from.readAs<duint8>(d->format);

    if (d->format == UseQImageFormat)
    {
        Block block;
        from >> block;
        QDataStream is(block);
        is.setVersion(QDataStream::Qt_4_8);
        is >> d->image;

        d->size.x = d->image.width();
        d->size.y = d->image.height();
    }
    else
    {
        from >> d->size >> d->pixels;
    }
}

GLPixelFormat Image::glFormat(Format imageFormat)
{
    DENG2_ASSERT(imageFormat >= Luminance_8 && imageFormat <= RGBx_8888);

    switch (imageFormat)
    {
    case Luminance_8:
        return GLPixelFormat(GL_LUMINANCE, GL_UNSIGNED_BYTE, 1);

    case LuminanceAlpha_88:
        return GLPixelFormat(GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, 2);

    case Alpha_8:
        return GLPixelFormat(GL_ALPHA, GL_UNSIGNED_BYTE, 1);

    case RGB_555:
        return GLPixelFormat(GL_RGB, GL_UNSIGNED_SHORT_5_5_5_1, 2);

    case RGB_565:
        return GLPixelFormat(GL_RGB, GL_UNSIGNED_SHORT_5_6_5, 2);

    case RGB_444:
        return GLPixelFormat(GL_RGB, GL_UNSIGNED_SHORT_4_4_4_4, 2);

    case RGB_888:
        return GLPixelFormat(GL_RGB, GL_UNSIGNED_BYTE, 1);

    case RGBA_4444:
        return GLPixelFormat(GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, 2);

    case RGBA_5551:
        return GLPixelFormat(GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, 2);

    case RGBA_8888:
        return GLPixelFormat(GL_RGBA, GL_UNSIGNED_BYTE, 4);

    default:
    case RGBx_8888:
        return GLPixelFormat(GL_RGBA, GL_UNSIGNED_BYTE, 4);
    }
}

GLPixelFormat Image::glFormat(QImage::Format format)
{
    switch (format)
    {
    case QImage::Format_Indexed8:
        return GLPixelFormat(GL_LUMINANCE, GL_UNSIGNED_BYTE, 1);

    case QImage::Format_RGB444:
        return GLPixelFormat(GL_RGB, GL_UNSIGNED_SHORT_4_4_4_4, 2);

    case QImage::Format_ARGB4444_Premultiplied:
        return GLPixelFormat(GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, 2);

    case QImage::Format_RGB555:
        return GLPixelFormat(GL_RGB, GL_UNSIGNED_SHORT_5_5_5_1, 2);

    case QImage::Format_RGB16:
        return GLPixelFormat(GL_RGB, GL_UNSIGNED_SHORT_5_6_5, 2);

    case QImage::Format_RGB888:
        return GLPixelFormat(GL_RGB, GL_UNSIGNED_BYTE, 1);

    case QImage::Format_RGB32:
#if defined (DENG_OPENGL)
        /// @todo Is GL_BGR in any GL standard spec? Check for EXT_bgra.
        return GLPixelFormat(GL_BGR, GL_UNSIGNED_BYTE, 4);
#else
        return GLPixelFormat(GL_BGRA8_EXT, GL_UNSIGNED_BYTE, 4);
#endif

    case QImage::Format_ARGB32:
        /// @todo Is GL_BGRA in any GL standard spec? Check for EXT_bgra.
        return GLPixelFormat(GL_BGRA, GL_UNSIGNED_BYTE, 4);

    default:
        break;
    }
    return GLPixelFormat(GL_RGBA, GL_UNSIGNED_BYTE, 4);
}

Image Image::solidColor(Color const &color, Size const &size)
{
    QImage img(QSize(size.x, size.y), QImage::Format_ARGB32);
    img.fill(QColor(color.x, color.y, color.z, color.w).rgba());
    return img;
}

Image Image::fromData(IByteArray const &data, String const &formatHint)
{
    return fromData(Block(data), formatHint);
}

Image Image::fromData(Block const &data, String const &formatHint)
{
    // Targa doesn't have a reliable "magic" identifier so we require a hint.
    if (!formatHint.compareWithoutCase(".tga") && tga::recognize(data))
    {
        return tga::load(data);
    }

    // Qt does not support PCX images (too old school?).
    if (pcx::recognize(data))
    {
        return pcx::load(data);
    }

    /// @todo Could check when alpha channel isn't needed and return an RGB888
    /// image instead. -jk
    return QImage::fromData(data).convertToFormat(QImage::Format_ARGB32);
}

Image Image::fromIndexedData(Size const &size, IByteArray const &image, IByteArray const &palette)
{
    QImage rgba(size.x, size.y, QImage::Format_ARGB32);

    // Process the source data by row.
    Block color(size.x);
    for (duint y = 0; y < size.y; ++y)
    {
        duint32 *dest = reinterpret_cast<duint32 *>(rgba.bits() + y * rgba.bytesPerLine());

        image.get(size.x * y, color.data(), color.size());
        auto const *srcColor = color.dataConst();

        for (duint x = 0; x < size.x; ++x)
        {
            duint8 paletteColor[3];
            palette.get(*srcColor++ * 3, paletteColor, 3);
            *dest++ = qRgba(paletteColor[0], paletteColor[1], paletteColor[2], 255);
        }
    }

    return rgba;
}

Image Image::fromMaskedIndexedData(Size const &size, IByteArray const &imageAndMask,
                                   IByteArray const &palette)
{
    duint const layerSize = size.x * size.y;

    QImage rgba(size.x, size.y, QImage::Format_ARGB32);

    // Process the source data by row.
    Block color(size.x);
    Block alpha(size.x);

    for (duint y = 0; y < size.y; ++y)
    {
        duint32 *dest = reinterpret_cast<duint32 *>(rgba.bits() + y * rgba.bytesPerLine());

        imageAndMask.get(size.x * y,             color.data(), color.size());
        imageAndMask.get(size.x * y + layerSize, alpha.data(), alpha.size());

        auto const *srcColor = color.dataConst();
        auto const *srcAlpha = alpha.dataConst();

        for (duint x = 0; x < size.x; ++x)
        {
            duint8 paletteColor[3];
            palette.get(*srcColor++ * 3, paletteColor, 3);
            *dest++ = qRgba(paletteColor[0], paletteColor[1], paletteColor[2], *srcAlpha++);
        }
    }

    return rgba;
}

bool Image::recognize(File const &file)
{
    String const ext = file.extension().toLower();
    return (ext == ".tga"  || ext == ".pcx" || ext == ".png"  || ext == ".jpg" ||
            ext == ".jpeg" || ext == ".gif" || ext == ".tiff" || ext == ".ico");
}

} // namespace de
