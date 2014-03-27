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

#include "de/Image"
#include "de/graphics/opengl.h"
#include <de/Reader>
#include <de/Writer>
#include <de/Block>
#include <de/FixedByteArray>

#include <QDataStream>
#include <QPainter>

namespace de {

#define IMAGE_ASSERT_EDITABLE(d) DENG2_ASSERT(d->format == UseQImageFormat)

namespace internal {

static dbyte const PCX_MAGIC        = 0x0a;
static dbyte const PCX_RLE_ENCODING = 1;
static dsize const PCX_HEADER_SIZE  = 128;

struct PCXHeader : public IReadable
{
    dbyte magic;
    dbyte version;
    dbyte encoding;
    dbyte bitsPerPixel;
    duint16 xMin, yMin;
    duint16 xMax, yMax;
    duint16 hRes, vRes;
    dbyte colorPlanes;
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

static bool isPCXFormat(Block const &data)
{
    try
    {
        PCXHeader header;
        Reader(data) >> header;

        // Only paletted format supported.
        return (header.magic == PCX_MAGIC &&
                header.version == 5 /* latest */ &&
                header.encoding == PCX_RLE_ENCODING &&
                header.bitsPerPixel == 8);
    }
    catch(Error const &)
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
static QImage loadPCX(Block const &data)
{
    PCXHeader header;
    Reader reader(data);
    reader >> header;

    Image::Size const size(header.xMax + 1, header.yMax + 1);

    QImage image(size.x, size.y, QImage::Format_RGB888);
    DENG2_ASSERT(image.depth() == 24);

    dbyte const *palette = data.data() + data.size() - 768;
    dbyte const *pos = data.data() + PCX_HEADER_SIZE;
    dbyte *dst = image.bits();

    for(duint y = 0; y < size.y; ++y, dst += size.x * 3)
    {
        for(duint x = 0; x < size.x; )
        {
            dbyte value = *pos++;

            // RLE inflation.
            int runLength = 1;
            if((value & 0xc0) == 0xc0)
            {
                runLength = value & 0x3f;
                value = *pos++;
            }
            while(runLength-- > 0)
            {
                // Get the RGB triplets from the palette.
                std::memcpy(&dst[3 * x++], &palette[3 * value], 3);
            }
        }
    }

    return image;
}

} // namespace internal
using namespace internal;

DENG2_PIMPL(Image)
{
    Format format;
    Size size;
    QImage image;
    Block pixels;
    ByteRefArray refPixels;

    Instance(Public *i, QImage const &img = QImage())
        : Base(i), format(UseQImageFormat), image(img)
    {
        size = Size(img.width(), img.height());
    }

    Instance(Public *i, Instance const &other)
        : Base     (i),
          format   (other.format),
          size     (other.size),
          image    (other.image),
          pixels   (other.pixels),
          refPixels(other.refPixels)
    {}

    Instance(Public *i, Size const &imgSize, Format imgFormat, IByteArray const &imgPixels)
        : Base(i), format(imgFormat), size(imgSize), pixels(imgPixels)
    {}

    Instance(Public *i, Size const &imgSize, Format imgFormat, ByteRefArray const &imgRefPixels)
        : Base(i), format(imgFormat), size(imgSize), refPixels(imgRefPixels)
    {}   
};

Image::Image() : d(new Instance(this))
{}

Image::Image(Image const &other) : d(new Instance(this, *other.d))
{}

Image::Image(QImage const &image) : d(new Instance(this, image))
{}

Image::Image(Size const &size, Format format, IByteArray const &pixels)
    : d(new Instance(this, size, format, pixels))
{}

Image::Image(Size const &size, Format format, ByteRefArray const &refPixels)
    : d(new Instance(this, size, format, refPixels))
{}

Image &Image::operator = (Image const &other)
{
    d.reset(new Instance(this, *other.d));
    return *this;
}

Image &Image::operator = (QImage const &other)
{
    d.reset(new Instance(this, other));
    return *this;
}

Image::Format Image::format() const
{
    return d->format;
}

QImage::Format Image::qtFormat() const
{
    if(d->format == UseQImageFormat)
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
    switch(d->format)
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
    if(d->format == UseQImageFormat)
    {
        return d->image.bytesPerLine();
    }
    return depth()/8 * d->size.x;
}

int Image::byteCount() const
{
    if(d->format == UseQImageFormat)
    {
        return d->image.byteCount();
    }
    if(!d->pixels.isEmpty())
    {
        return d->pixels.size();
    }
    return depth()/8 * d->size.x * d->size.y;
}

void const *Image::bits() const
{
    if(d->format == UseQImageFormat)
    {
        return d->image.constBits();
    }
    if(!d->pixels.isEmpty())
    {
        return d->pixels.constData();
    }
    return d->refPixels.readBase();
}

void *Image::bits()
{
    if(d->format == UseQImageFormat)
    {
        return d->image.bits();
    }
    if(!d->pixels.isEmpty())
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
    if(d->format == UseQImageFormat)
    {
        // Some QImage formats are GL compatible.
        switch(qtFormat())
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
    switch(d->format)
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
    if(d->format == UseQImageFormat)
    {
        return d->image;
    }

    // There may be some conversions we can do.
    QImage::Format form = QImage::Format_Invalid;
    switch(d->format)
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
    if(d->format == UseQImageFormat)
    {
        return glFormat(d->image.format());
    }
    return glFormat(d->format);
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
    QPainter painter(&resized);
    painter.drawImage(QPoint(0, 0), d->image);
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

void Image::operator >> (Writer &to) const
{
    to << duint8(d->format);

    if(d->format == UseQImageFormat)
    {
        Block block;
        QDataStream os(&block, QIODevice::WriteOnly);
        os.setVersion(QDataStream::Qt_4_7);
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

    if(d->format == UseQImageFormat)
    {
        Block block;
        from >> block;
        QDataStream is(block);
        is.setVersion(QDataStream::Qt_4_7);
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

    switch(imageFormat)
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
    switch(format)
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
        /// @todo Is GL_BGR in any GL standard spec? Check for EXT_bgra.
        return GLPixelFormat(GL_BGR, GL_UNSIGNED_BYTE, 4);

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

Image Image::fromData(IByteArray const &data)
{
    return fromData(Block(data));
}

Image Image::fromData(Block const &data)
{
    if(isPCXFormat(data))
    {
        // Qt does not support PCX images (too old school?).
        return loadPCX(data);
    }
    /// @todo Could check when alpha channel isn't needed and return an RGB888
    /// image instead. -jk
    return QImage::fromData(data).convertToFormat(QImage::Format_ARGB32);
}

} // namespace de
