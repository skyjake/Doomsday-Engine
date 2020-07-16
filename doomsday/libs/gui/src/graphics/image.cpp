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

#include "de/image.h"
#include "de/opengl.h"

#include <de/block.h>
#include <de/file.h>
#include <de/fixedbytearray.h>
#include <de/log.h>
#include <de/reader.h>
#include <de/vector.h>
#include <de/writer.h>
#include <de/zeroed.h>

#include <stb/stb_image.h>
#include <stb/stb_image_write.h>
#include <stb/stb_image_resize.h>

#include <map>

namespace de {

static constexpr int JPEG_QUALITY = 85;

#define IMAGE_ASSERT_EDITABLE(d) DE_ASSERT(d->format == RGBA_8888)

namespace internal {

namespace pcx {

static const dbyte MAGIC        = 0x0a;
static const dbyte RLE_ENCODING = 1;
static const dsize HEADER_SIZE  = 128;

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

static bool recognize(const Block &data)
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
    catch (const Error &)
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
static Image load(const Block &data)
{
    Header header;
    Reader reader(data);
    reader >> header;

    Image::Size const size(header.xMax + 1, header.yMax + 1);

    Image image(size, Image::RGB_888);
    DE_ASSERT(image.depth() == 24);

    const dbyte *palette = data.data() + data.size() - 768;
    const dbyte *pos = data.data() + HEADER_SIZE;
    dbyte *dst = reinterpret_cast<dbyte *>(image.bits());

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
    enum Flag {
        NoFlags           = 0,
        ScreenOriginUpper = 0x1,
        InterleaveTwoWay  = 0x2,
        InterleaveFourWay = 0x4
    };

    enum ColorMapType {
        ColorMapNone = 0,
        ColorMap256  = 1 // not supported
    };

    enum ImageType {
        ColorMapped = 1,  // Uncompressed and color-mapped.
        RGB         = 2,  // Uncompressed RGB.
        RleRGB      = 10, // Run length encoded RGB.
    };

    Block identification;
    Uint8 colorMapType;
    Uint8 imageType;

    // Color map.
    Int16 mapIndex;
    Int16 mapCount;        ///< Number of color map entries.
    Uint8 mapEntrySize;    ///< Bits in a color map entry.

    // Image specification.
    Flags flags;
    Vector2<dint16> origin;
    Vector2<dint16> size;
    Uint8 depth;
    Uint8 attrib;

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

static bool recognize(const Block &data)
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

static Image load(const Block &data)
{
    Header header;
    Reader input(data);
    input >> header;

    const int pixelSize = header.depth / 8;
    Image img(Image::Size(header.size.x, header.size.y),
              pixelSize == 4? Image::RGBA_8888 : Image::RGB_888);
    dbyte *base = reinterpret_cast<dbyte *>(img.bits());

    const bool isUpperOrigin = header.flags.testFlag(Header::ScreenOriginUpper);

    // RGB can be read line by line.
    if (header.imageType == Header::RGB)
    {
        for (int y = 0; y < header.size.y; y++)
        {
            int inY = (isUpperOrigin? y : (header.size.y - y - 1));
            ByteRefArray line(base + (inY * img.stride()), header.size.x * pixelSize);
            input.readBytesFixedSize(line);
        }
    }
    else if (header.imageType == Header::RleRGB)
    {
        img.fill(Image::Color());

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
        DE_ASSERT(header.colorMapType == Header::ColorMap256);
        DE_ASSERT(header.depth == 8);

        // Read the colormap.
        std::vector<Image::Color> colorTable(256);
        for (int i = 0; i < header.mapCount; ++i)
        {
            uint8_t color[4] = {0, 0, 0, 255};
            ByteRefArray buf(color, 4);
            input.readBytes(header.mapEntrySize / 8, buf);
            DE_ASSERT(header.mapIndex + i < 256);
            colorTable[header.mapIndex + i] = {color[2], color[1], color[0], color[3]};
        }

        img = Image(Image::Size(header.size.x, header.size.y), Image::RGBA_8888);

        Block lineBuf(header.size.x);
        for (int y = 0; y < header.size.y; y++)
        {           
            input.readBytesFixedSize(lineBuf);

            const int inY = (isUpperOrigin ? y : (header.size.y - y - 1));
            uint8_t * idx = lineBuf.data();
            for (uint32_t *px = img.row32(inY); px != img.rowEnd32(inY); ++px)
            {
                *px = Image::packColor(colorTable[*idx++]);
            }
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

DE_PIMPL(Image)
{
    Format       format;
    Size         size;
    Block        pixels;
    ByteRefArray refPixels;
    float        pointRatio = 1.f;
    Vec2i        origin;

    Impl(Public *i)
        : Base(i)
        , format(Unknown)
    {}

    Impl(Public *i, const Impl &other)
        : Base(i)
        , format(other.format)
        , size(other.size)
        , pixels(other.pixels)
        , refPixels(other.refPixels)
        , pointRatio(other.pointRatio)
        , origin(other.origin)
    {}

    Impl(Public *i, const Size &imgSize, Format imgFormat)
        : Base(i)
        , format(imgFormat)
        , size(imgSize)
    {}

    Impl(Public *i, const Size &imgSize, Format imgFormat, const IByteArray &imgPixels)
        : Base(i)
        , format(imgFormat)
        , size(imgSize)
        , pixels(imgPixels)
    {}

    Impl(Public *i, const Size &imgSize, Format imgFormat, const Block &imgPixels)
        : Base(i)
        , format(imgFormat)
        , size(imgSize)
        , pixels(imgPixels)
    {}

    Impl(Public *i, const Size &imgSize, Format imgFormat, const ByteRefArray &imgRefPixels)
        : Base(i)
        , format(imgFormat)
        , size(imgSize)
        , refPixels(imgRefPixels)
    {}

    Rectanglei rect() const
    {
        return Rectanglei::fromSize(size);
    }
};

Image::Image() : d(new Impl(this))
{}

Image::Image(const Image &other)
    : d(new Impl(this, *other.d))
{}

Image::Image(Image &&moved)
    : d(std::move(moved.d))
{}

Image::Image(const Size &size, Format format)
    : d(new Impl(this, size, format))
{
    d->pixels.resize(stride() * size.y);
}

Image::Image(const Size &size, Format format, const Block &pixels)
    : d(new Impl(this, size, format, pixels))
{}
    
Image::Image(const Size &size, Format format, const IByteArray &pixels)
    : d(new Impl(this, size, format, pixels))
{}

Image::Image(const Size &size, Format format, const ByteRefArray &refPixels)
    : d(new Impl(this, size, format, refPixels))
{}

Image &Image::operator=(const Image &other)
{
    d.reset(new Impl(this, *other.d));
    return *this;
}

Image &Image::operator=(Image &&moved)
{
    std::swap(d, moved.d);
    return *this;
}

//Image &Image::operator = (const QImage &other)
//{
//    d.reset(new Impl(this, other));
//    return *this;
//}

Image::Format Image::format() const
{
    return d->format;
}

//QImage::Format Image::qtFormat() const
//{
//    if (d->format == UseQImageFormat)
//    {
//        return d->image.format();
//    }
//    return QImage::Format_Invalid;
//}

Image::Size Image::size() const
{
    return d->size;
}

Rectanglei Image::rect() const
{
    return Rectanglei(0, 0, d->size.x, d->size.y);
}

duint Image::depth() const
{
    switch (d->format)
    {
//    case UseQImageFormat: return d->image.depth();

    case Luminance_8:
    case Alpha_8: return 8;

    case LuminanceAlpha_88:
    case RGB_565:
    case RGBA_4444:
    case RGBA_5551: return 16;

    case RGB_888: return 24;

    case RGBA_8888:
    case RGBx_8888: return 32;

    case R_16f:    return 16;
    case RG_16f:   return 32;
    case RGB_16f:  return 48;
    case RGBA_16f: return 64;

    case R_32f:
    case R_32i:
    case R_32ui: return 32;

    case RG_32f:
    case RG_32i:
    case RG_32ui: return 64;

    case RGB_32f:
    case RGB_32i:
    case RGB_32ui: return 96;

    case RGBA_32f:
    case RGBA_32i:
    case RGBA_32ui: return 128;

    default: return 0;
    }
}

dsize Image::stride() const
{
    return bytesPerPixel() * d->size.x;
}

dsize Image::byteCount() const
{
    if (!d->pixels.isEmpty())
    {
        return d->pixels.size();
    }
    return bytesPerPixel() * d->size.x * d->size.y;
}

const dbyte *Image::bits() const
{
    if (!d->pixels.isEmpty())
    {
        return d->pixels.constData();
    }
    return reinterpret_cast<const dbyte *>(d->refPixels.readBase());
}

dbyte *Image::bits()
{
    if (!d->pixels.isEmpty())
    {
        return d->pixels.data();
    }
    return reinterpret_cast<dbyte *>(d->refPixels.base());
}

const dbyte *Image::row(duint y) const
{
    DE_ASSERT(y < height());
    return bits() + stride() * y;
}

dbyte *Image::row(duint y)
{
    DE_ASSERT(y < height());
    return bits() + stride() * y;
}

dbyte *Image::rowEnd(duint y)
{
    return row(y) + width() * bytesPerPixel();
}

const duint32 *Image::row32(duint y) const
{
    return reinterpret_cast<const duint32 *>(row(y));
}

duint32 *Image::row32(duint y)
{
    return reinterpret_cast<duint32 *>(row(y));
}

duint32 *Image::rowEnd32(duint y)
{
    return reinterpret_cast<duint32 *>(row(y) + width() * bytesPerPixel());
}

bool Image::isNull() const
{
    return size().area() == 0;
}

bool Image::isGLCompatible() const
{
//    if (d->format == UseQImageFormat)
//    {
//        // Some QImage formats are GL compatible.
//        switch (qtFormat())
//        {
//        case QImage::Format_ARGB32: // 8888
//        case QImage::Format_RGB32:  // 8888
//        case QImage::Format_RGB888: // 888
//        case QImage::Format_RGB16:  // 565
//        case QImage::Format_RGB555: // 555
//        case QImage::Format_RGB444: // 444
//            return true;

//        default:
//            return false;
//        }
//    }
    return d->format >= Luminance_8 && d->format <= RGBA_32ui;
}

bool Image::hasAlphaChannel() const
{
    switch (d->format)
    {
        case LuminanceAlpha_88:
        case Alpha_8:
        case RGBA_4444:
        case RGBA_5551:
        case RGBA_8888:
        case RGBA_16f:
        case RGBA_32f:
        case RGBA_32i:
        case RGBA_32ui:
            return true;

        default:
            return false;
    }
}

Image Image::convertToFormat(Format toFormat) const
{
    if (d->format == toFormat)
    {
        // No conversion necessary.
        return *this;
    }
    const int inStep = bytesPerPixel();
    Image conv(size(), toFormat);
    if (d->format == Luminance_8 && toFormat == RGBA_8888)
    {
        for (duint y = 0; y < height(); ++y)
        {
            const duint8 *in = row(y);
            for (duint32 *out = conv.row32(y), *outEnd = conv.rowEnd32(y);
                 out != outEnd; ++out, ++in)
            {
                *out = packColor(Color(*in, *in, *in, 255));
            }
        }
        return conv;
    }
    if (d->format == RGB_888 && toFormat == RGBA_8888)
    {
        for (duint y = 0; y < height(); ++y)
        {
            const duint8 *in = row(y);
            for (duint32 *out = conv.row32(y), *outEnd = conv.rowEnd32(y);
                 out != outEnd; ++out, in += inStep)
            {
                *out = packColor(Color(in[0], in[1], in[2], 255));
            }
        }
        return conv;
    }
    DE_ASSERT_FAIL("Image::convertToFormat not implemented for the given input/output formats");
    return conv;
}

GLPixelFormat Image::glFormat() const
{
    return glFormat(d->format);
}

float Image::pointRatio() const
{
    return d->pointRatio;
}

void Image::setOrigin(const Vec2i &origin)
{
    d->origin = origin;
}

Vec2i Image::origin() const
{
    return d->origin;
}

Image::Color Image::pixel(Vec2ui pos) const
{
    const dbyte *ptr = &row(pos.y)[pos.x * bytesPerPixel()];
    switch (d->format)
    {
    case RGBA_8888: return Color(ptr[0], ptr[1], ptr[2], ptr[3]);
    case RGB_888:   return Color(ptr[0], ptr[1], ptr[2]);

    default: DE_ASSERT_FAIL("Image::pixel does not support this format"); break;
    }
    return {};
}

void Image::setPointRatio(float pointsPerPixel)
{
    d->pointRatio = pointsPerPixel;
}

Image Image::subImage(const Rectanglei &subArea) const
{
    const dsize bpp    = bytesPerPixel();
    const auto  bounds = d->rect() & subArea;
    Image sub(bounds.size(), d->format);
    for (duint y = 0; y < bounds.height(); ++y)
    {
        memcpy(sub.row(y), row(bounds.top() + y) + bounds.left() * bpp, bounds.width() * bpp);
    }
    return sub;
}

void Image::resize(const Size &size)
{
    DE_ASSERT(d->format == RGB_888 || d->format == RGBA_8888);

    Image resized{size, d->format};
    stbir_resize_uint8(bits(),
                       width(),
                       height(),
                       stride(),
                       resized.bits(),
                       size.x,
                       size.y,
                       resized.stride(),
                       bytesPerPixel());
    *this = resized;
}

void Image::fill(Color color)
{
    IMAGE_ASSERT_EDITABLE(d);

    const duint32 colorBits = packColor(color);
    for (duint y = 0; y < height(); ++y)
    {
        for (duint32 *i = row32(y), *end = rowEnd32(y); i != end; ++i)
        {
            *i = colorBits;
        }
    }
}

void Image::fill(const Rectanglei &rect, Color color)
{
    IMAGE_ASSERT_EDITABLE(d);

    const Rectanglei bounds = d->rect() & rect;
    const duint32 colorBits = packColor(color);

    for (int y = bounds.top(); y < bounds.bottom(); ++y)
    {
        auto *rowStart = row32(y) + bounds.left();
        auto *end = rowStart + bounds.width();
        for (duint32 *i = rowStart; i != end; ++i)
        {
            *i = colorBits;
        }
    }
}

void Image::setPixel(Vec2ui pos, Color color)
{
    IMAGE_ASSERT_EDITABLE(d);

    uint32_t packed = packColor(color);
    std::memcpy(&row(pos.y)[bytesPerPixel() * pos.x], &packed, 4);
}

void Image::drawRect(const Rectanglei &rect, Color color)
{
    IMAGE_ASSERT_EDITABLE(d);

    if (rect.isNull()) return;

    const duint32 packed = packColor(color);

    duint32 *top    = row32(rect.top())        + rect.left();
    duint32 *bottom = row32(rect.bottom() - 1) + rect.left();
    for (duint x = 0; x < rect.width(); ++x)
    {
        *top++    = packed;
        *bottom++ = packed;
    }

    const int stride32 = stride() / 4;

    duint32 *left  = row32(rect.top() + 1) + rect.left();
    duint32 *right = row32(rect.top() + 1) + (rect.right() - 1);
    for (duint y = 0; y < rect.height() - 2; ++y)
    {
        *left = *right = packed;
        left  += stride32;
        right += stride32;
    }
}

void Image::draw(const Image &image, const Vec2i &topLeft)
{
    drawPartial(image, image.d->rect(), topLeft);
}

void Image::drawPartial(const Image &image, const Rectanglei &part, const Vec2i &topLeft)
{
    DE_ASSERT(d->format == image.d->format); // conversion not supported
    IMAGE_ASSERT_EDITABLE(d);
    IMAGE_ASSERT_EDITABLE(image.d);

    const auto srcPart  = image.d->rect() & part;
    const auto bounds   = Rectanglei(topLeft, topLeft + srcPart.size().toVec2i());
    const auto destRect = d->rect() & bounds;
    const auto srcRect  = destRect.moved(-topLeft);

    if (srcRect.isNull()) return;

    DE_ASSERT(srcRect.size() == destRect.size());

    for (int dy = destRect.top(), sy = srcRect.top(); sy < srcRect.bottom(); ++dy, ++sy)
    {
        memcpy(row32(dy) + destRect.left(),
               image.row32(sy) + srcRect.left(),
               srcRect.width() * bytesPerPixel());
    }
}

Image Image::multiplied(const Image &factorImage) const
{
    IMAGE_ASSERT_EDITABLE(factorImage.d);

    const auto bounds = d->rect() & factorImage.d->rect();

    Image multiplied = convertToFormat(RGBA_8888);
    for (duint y = 0; y < bounds.height(); ++y)
    {
        const duint32 *src1 = row32(y);
        const duint32 *end  = src1 + bounds.width();
        const duint32 *src2 = factorImage.row32(y);

        for (duint32 *dest = multiplied.row32(y); src1 != end; ++src1, ++src2, ++dest)
{
            const Color16 col1 = unpackColor16(*src1);
            const Color16 col2 = unpackColor16(*src2);
            *dest = packColor(Color16((col2.x + 1) * col1.x >> 8,
                                      (col2.y + 1) * col1.y >> 8,
                                      (col2.z + 1) * col1.z >> 8,
                                      (col2.w + 1) * col1.w >> 8));
        }
    }
    return multiplied;
}

Image Image::multiplied(Color color) const
{
    if (color == Color(255, 255, 255, 255)) return *this; // No change.

    Image copy = convertToFormat(RGBA_8888);

    for (duint y = 0; y < height(); ++y)
    {
        duint32 *ptr = reinterpret_cast<duint32 *>(copy.bits() + y * copy.stride());
        for (duint x = 0; x < width(); ++x)
        {
            const Color16 pix = unpackColor16(*ptr);
            *ptr++ = packColor(Color16((color.x + 1) * pix.x >> 8,
                                       (color.y + 1) * pix.y >> 8,
                                       (color.z + 1) * pix.z >> 8,
                                       (color.w + 1) * pix.w >> 8));
        }
    }
    return copy;
}

Image Image::colorized(Color color) const
{
    const float targetHue = hsv(color).x;

    Image copy = convertToFormat(RGBA_8888);
    for (duint y = 0; y < height(); ++y)
    {
        for (duint32 *ptr = copy.row32(y), *end = ptr + width(); ptr != end; ++ptr)
        {
            Vec4f pixelHsv = hsv(unpackColor(*ptr));

            pixelHsv.x = targetHue;
            pixelHsv.w = (color.w / 255.f) * pixelHsv.w;

            *ptr = packColor(fromHsv(pixelHsv));
        }
    }
    return copy;
}

Image Image::invertedColor() const
{
    Image img = convertToFormat(RGBA_8888);
    for (duint y = 0; y < height(); ++y)
    {
        for (duint32 *ptr = img.row32(y), *end = ptr + width(); ptr != end; ++ptr)
        {
            const auto color = unpackColor(*ptr);
            *ptr = packColor(Color(255 - color.x, 255 - color.y, 255 - color.z, color.w));
        }
    }
    return img;
}

Image Image::mixed(const Image &low, const Image &high) const
{
    DE_ASSERT(size() == low.size());
    DE_ASSERT(size() == high.size());

    const Image &lowImg  = low;
    const Image &highImg = high;

    Image mix = convertToFormat(RGBA_8888);
    for (duint y = 0; y < height(); ++y)
    {
        duint32 *ptr = mix.row32(y);
        for (duint x = 0; x < width(); ++x)
        {
            const auto pix = unpackColor(*ptr);

            const duint mr = pix.x;
            const duint mg = pix.y;
            const duint mb = pix.z;
            const duint ma = pix.w;

            const auto lowColor  = lowImg .pixel(x, y);
            const auto highColor = highImg.pixel(x, y);

            int red   = (highColor.x * mr + lowColor.x * (255 - mr)) / 255;
            int green = (highColor.y * mg + lowColor.y * (255 - mg)) / 255;
            int blue  = (highColor.z * mb + lowColor.z * (255 - mb)) / 255;
            int alpha = (highColor.w * ma + lowColor.w * (255 - ma)) / 255;

            *ptr = packColor(makeColor(red, green, blue, alpha));

            ptr++;
        }
    }
    return mix;
}
    
Image Image::mixed(const Color &zero, const Color &one, const int componentIndices[4]) const
{
    static const int defaultIndices[] = { 0, 1, 2, 3 };
    const int *comps = (componentIndices ? componentIndices : defaultIndices);
    Image mix = convertToFormat(RGBA_8888);
    for (duint y = 0; y < height(); ++y)
    {
        duint32 *ptr = mix.row32(y);
        for (duint x = 0; x < width(); ++x)
        {
            const auto pix = unpackColor(*ptr);

            const duint mr = pix[comps[0]];
            const duint mg = pix[comps[1]];
            const duint mb = pix[comps[2]];
            const duint ma = pix[comps[3]];

            int red   = (one.x * mr + zero.x * (255 - mr)) / 255;
            int green = (one.y * mg + zero.y * (255 - mg)) / 255;
            int blue  = (one.z * mb + zero.z * (255 - mb)) / 255;
            int alpha = (one.w * ma + zero.w * (255 - ma)) / 255;
            
            *ptr = packColor(makeColor(red, green, blue, alpha));
            
            ptr++;
        }
    }
    return mix;
}

Image Image::withAlpha(const Image &grayscale) const
{
    DE_ASSERT(size() == grayscale.size());
    const Image &alpha = grayscale;
    Image img = convertToFormat(RGBA_8888);
    for (duint y = 0; y < height(); ++y)
    {
        duint32 *ptr = img.row32(y);
        for (duint x = 0; x < width(); ++x)
        {
            *ptr &= 0x00ffffff;
            *ptr++ |= alpha.pixel(x, y).x << 24;
        }
    }
    return img;
}

Image Image::rgbSwapped() const
{
    DE_ASSERT_FAIL("Image::rgbSwapped is not implemented");
    return {};
}

Image Image::flipped() const
{
    Image flip(d->size, d->format);
    for (duint y = 0; y < height(); ++y)
    {
        memcpy(flip.row(y), row(height() - 1 - y), stride());
    }
    return flip;
}

Image Image::solidColor(Color color, const Size &size)
{
    Image img(size, Image::RGBA_8888);
    img.fill(color);
    return img;
}

void Image::save(const NativePath &path) const
{
    DE_ASSERT(d->format == RGBA_8888 || d->format == RGB_888 ||
              d->format == Luminance_8 || d->format == LuminanceAlpha_88);

    const String ext = path.toString().fileNameExtension();
    const int comp = d->format == RGBA_8888 ? 4
                   : d->format == RGB_888 ? 3
                   : d->format == LuminanceAlpha_88 ? 2 : 1;

    if (!ext.compareWithoutCase(".png"))
    {
        stbi_write_png(path.c_str(), width(), height(), comp, bits(), stride());
    }
    else if (!ext.compareWithoutCase(".jpg"))
    {
        stbi_write_jpg(path.c_str(), width(), height(), comp, bits(), JPEG_QUALITY);
    }
    else if (!ext.compareWithoutCase(".tga"))
    {
        stbi_write_tga(path.c_str(), width(), height(), comp, bits());
    }
    else if (!ext.compareWithoutCase(".bmp"))
    {
        stbi_write_bmp(path.c_str(), width(), height(), comp, bits());
    }
    else
    {
        throw Error("Image::save", "Image format " + ext + " not supported for writing");
    }
}

static void dataWriter(void *context, void *data, int size)
{
    reinterpret_cast<Block *>(context)->append(data, size);
}

Block Image::serialize(SerializationFormat format) const
{
    Block data;
    const int comp = bytesPerPixel();
    switch (format)
    {
    case Png:
        stbi_write_png_to_func(dataWriter, &data, width(), height(), comp, bits(), stride());
        break;

    case Jpeg:
        stbi_write_jpg_to_func(dataWriter, &data, width(), height(), comp, bits(), JPEG_QUALITY);
        break;

    case Targa:
        stbi_write_tga_to_func(dataWriter, &data, width(), height(), comp, bits());
        break;

    case Bmp:
        stbi_write_bmp_to_func(dataWriter, &data, width(), height(), comp, bits());
        break;
    }
    return data;
}

Block Image::serialize(const char *formatHint) const
{
    struct Hint {
        const char *ext;
        SerializationFormat sformat;
    };
    static const Hint hints[] = {
        { ".png",  Png   },
        { ".jpg",  Jpeg  },
        { ".jpeg", Jpeg  },
        { ".bmp",  Bmp   },
        { ".tga",  Targa },
    };

    for (const auto &hint : hints)
    {
        if (!iCmpStrCase(formatHint, hint.ext))
        {
            return serialize(hint.sformat);
        }
    }
    return serialize(Png);
}

void Image::operator>>(Writer &to) const
{
    to << duint8(d->format);

//    if (d->format == UseQImageFormat)
//    {
//        Block block;
//        QDataStream os(&block, QIODevice::WriteOnly);
//        os.setVersion(QDataStream::Qt_4_8);
//        os << d->image;
//        to << block;
//    }
//    else
    {
        to << d->size << ByteRefArray(bits(), byteCount());
    }
}

void Image::operator<<(Reader &from)
{
    d->pixels.clear();
    d->refPixels = ByteRefArray();

    from.readAs<duint8>(d->format);

//    if (d->format == UseQImageFormat)
//    {
//        Block block;
//        from >> block;
//        QDataStream is(block);
//        is.setVersion(QDataStream::Qt_4_8);
//        is >> d->image;

//        d->size.x = d->image.width();
//        d->size.y = d->image.height();
//    }
//    else
    {
        from >> d->size >> d->pixels;
    }
}

GLPixelFormat Image::glFormat(Format imageFormat) // static
{
    DE_ASSERT(imageFormat >= Luminance_8 && imageFormat <= RGBA_32ui);

    switch (imageFormat)
    {
    case Luminance_8:
        return GLPixelFormat(GL_R8, GL_RED, GL_UNSIGNED_BYTE, 1);

    case LuminanceAlpha_88:
        return GLPixelFormat(GL_RG8, GL_RG, GL_UNSIGNED_BYTE, 2);

    case Alpha_8:
        return GLPixelFormat(GL_R8, GL_ALPHA, GL_UNSIGNED_BYTE, 1);

    case RGB_555:
        return GLPixelFormat(GL_RGB5, GL_RGB, GL_UNSIGNED_SHORT_5_5_5_1, 2);

    case RGB_565:
        return GLPixelFormat(GL_RGB5, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, 2);

    case RGB_444:
        return GLPixelFormat(GL_RGB4, GL_RGB, GL_UNSIGNED_SHORT_4_4_4_4, 2);

    case RGB_888:
        return GLPixelFormat(GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE, 1);

    case RGBA_4444:
        return GLPixelFormat(GL_RGBA4, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, 2);

    case RGBA_5551:
        return GLPixelFormat(GL_RGB5_A1, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, 2);

    case RGBA_8888:
        return GLPixelFormat(GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 4);

    default:
    case RGBx_8888:
        return GLPixelFormat(GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 4);

    case R_8:
        return GLPixelFormat(GL_R8, GL_RED, GL_UNSIGNED_BYTE, 1);

    case RG_88:
        return GLPixelFormat(GL_RG8, GL_RG, GL_UNSIGNED_BYTE, 2);

    case R_16f:
        return GLPixelFormat(GL_R16F, GL_RED, GL_FLOAT, 4);

    case RG_16f:
        return GLPixelFormat(GL_RG16F, GL_RG, GL_FLOAT, 4);

    case RGB_16f:
        return GLPixelFormat(GL_RGB16F, GL_RGB, GL_FLOAT, 4);

    case RGBA_16f:
        return GLPixelFormat(GL_RGBA16F, GL_RGBA, GL_FLOAT, 4);

    case R_32f:
        return GLPixelFormat(GL_R32F, GL_RED, GL_FLOAT, 4);

    case RG_32f:
        return GLPixelFormat(GL_RG32F, GL_RG, GL_FLOAT, 4);

    case RGB_32f:
        return GLPixelFormat(GL_RGB32F, GL_RGB, GL_FLOAT, 4);

    case RGBA_32f:
        return GLPixelFormat(GL_RGBA32F, GL_RGBA, GL_FLOAT, 4);

    case R_32i:
        return GLPixelFormat(GL_R32I, GL_RED, GL_INT, 4);

    case RG_32i:
        return GLPixelFormat(GL_RG32I, GL_RG, GL_INT, 4);

    case RGB_32i:
        return GLPixelFormat(GL_RGB32I, GL_RGB, GL_INT, 4);

    case RGBA_32i:
        return GLPixelFormat(GL_RGBA32I, GL_RGBA, GL_INT, 4);

    case R_32ui:
        return GLPixelFormat(GL_R32UI, GL_RED, GL_UNSIGNED_INT, 4);

    case RG_32ui:
        return GLPixelFormat(GL_RG32UI, GL_RG, GL_UNSIGNED_INT, 4);

    case RGB_32ui:
        return GLPixelFormat(GL_RGB32UI, GL_RGB, GL_UNSIGNED_INT, 4);

    case RGBA_32ui:
        return GLPixelFormat(GL_RGBA32UI, GL_RGBA, GL_UNSIGNED_INT, 4);
    }
}

#if 0
GLPixelFormat Image::glFormat(QImage::Format format)
{
    switch (format)
    {
    case QImage::Format_Indexed8:
        return GLPixelFormat(GL_R8UI, GL_RED, GL_UNSIGNED_BYTE, 1);

    case QImage::Format_RGB444:
        return GLPixelFormat(GL_RGB4, GL_RGB, GL_UNSIGNED_SHORT_4_4_4_4, 2);

    case QImage::Format_ARGB4444_Premultiplied:
        return GLPixelFormat(GL_RGBA4, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, 2);

    case QImage::Format_RGB555:
        return GLPixelFormat(GL_RGB5, GL_RGB, GL_UNSIGNED_SHORT_5_5_5_1, 2);

    case QImage::Format_RGB16:
        return GLPixelFormat(GL_RGB5, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, 2);

    case QImage::Format_RGB888:
        return GLPixelFormat(GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE, 1);

    case QImage::Format_RGB32:
#if defined (DE_OPENGL)
        /// @todo Is GL_BGR in any GL standard spec? Check for EXT_bgra.
        return GLPixelFormat(GL_RGB8, GL_BGR, GL_UNSIGNED_BYTE, 4);
#else
        return GLPixelFormat(GL_BGRA8_EXT, GL_UNSIGNED_BYTE, 4);
#endif

    case QImage::Format_ARGB32:
        /// @todo Is GL_BGRA in any GL standard spec? Check for EXT_bgra.
        return GLPixelFormat(GL_RGBA8, GL_BGRA, GL_UNSIGNED_BYTE, 4);

    default:
        break;
    }
    return GLPixelFormat(GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 4);
}
#endif

Image Image::fromData(const IByteArray &data, const String &formatHint)
{
    return fromData(Block(data), formatHint);
}

Image Image::fromXpmData(const char * const *xpmStrings)
{
    duint width      = 0;
    duint height     = 0;
    int   colorCount = 0;
    int   perPixel   = 0;

    sscanf(*xpmStrings, "%d %d %d %d", &width, &height, &colorCount, &perPixel);
    DE_ASSERT(perPixel == 1);

    Color palette[127];
    for (int i = 0; i < colorCount; ++i)
    {
        const char *pal = xpmStrings[1 + i];
        const int   pc  = pal[0];
        const Color bgr = unpackColor(duint32(std::stoul(pal + 5, 0, 16)));
        DE_ASSERT(pc >= 0);
        palette[pc] = Color(bgr.z, bgr.y, bgr.x, 255);
    }
    Image xpm({width, height}, RGBA_8888);
    for (duint y = 0; y < height; ++y)
    {
        for (duint x = 0; x < width; ++x)
        {
            xpm.setPixel(x, y, palette[int((xpmStrings[1 + colorCount + y])[x])]);
        }
    }
    return xpm;
}

Image Image::fromData(const Block &data, const String &formatHint)
{
    // Targa doesn't have a reliable "magic" identifier so we require a hint.
    if (!formatHint.compareWithoutCase(".tga") && tga::recognize(data))
    {
        return tga::load(data);
    }

    if (pcx::recognize(data))
    {
        return pcx::load(data);
    }

    // STB provides readers for various formats.
    {
        int   w, h, num;
        auto *pixels = stbi_load_from_memory(data.data(), int(data.size()), &w, &h, &num, 0);
        if (pixels)
        {
            Image img;
            img.d->size   = Size(w, h);
            img.d->format = (num == 4 ? RGBA_8888 :
                             num == 3 ? RGB_888 :
                             num == 2 ? LuminanceAlpha_88 : Luminance_8);
            img.d->pixels = Block(pixels, num * w * h); // makes an extra copy...
            stbi_image_free(pixels);
            return img;
        }
    }

    return {};
}

Image Image::fromRgbaData(const Size &size, const IByteArray &rgba)
{
    const int rowLen = size.x * 4;
    Image img(size, Image::RGBA_8888);
    for (duint y = 0; y < size.y; ++y)
    {
        rgba.get(rowLen * y, img.row(y), rowLen);
    }
    return img;
}

Image Image::fromIndexedData(const Size &size, const IByteArray &image, const IByteArray &palette)
{
    return fromRgbaData(size, Block(image).mapAsIndices(3, palette, {{0, 0, 0, 255}}));
}

Image Image::fromMaskedIndexedData(const Size &size, const IByteArray &imageAndMask,
                                   const IByteArray &palette)
{
    const auto layerSize = imageAndMask.size() / 2;
    Block pixels = Block(imageAndMask, 0, layerSize)
            .mapAsIndices(3, palette, Block(imageAndMask, layerSize, layerSize));
    return fromRgbaData(size, pixels);
}

bool Image::recognize(const File &file)
{
    const String ext = file.extension();
    for (const char *e : {".tga", ".pcx", ".png", ".jpg", ".jpeg", ".bmp"})
    {
        if (!ext.compareWithoutCase(e)) return true;
    }
    return false;
}

//------------------------------------------------------------------------------------------------

Vec4f Image::hsv(Color color)
{
    const Vec4f rgb = color.toVec4f() / 255.f; // normalize

    Vec4f result(0, 0, 0, rgb.w); // alpha is unmodified

    float rgbMin = min(min(rgb.x, rgb.y), rgb.z);
    float rgbMax = max(max(rgb.x, rgb.y), rgb.z);
    float delta  = rgbMax - rgbMin;

    result.z = rgbMax;

    if (delta < 0.00001f)
    {
        result.x = 0;
        result.y = 0;
        return result;
    }

    if (rgbMax > 0.0f)
    {
        result.y = delta / rgbMax;
    }
    else
    {
        result.x = 0;
        result.y = 0;
        return result;
    }

    if (rgb.x >= rgbMax)
    {
        result.x = (rgb.y - rgb.z) / delta;
    }
    else
    {
        if (rgb.y >= rgbMax)
        {
            result.x = 2.0f + (rgb.z - rgb.x) / delta;
        }
        else
        {
            result.x = 4.0f + (rgb.x - rgb.y) / delta;
        }
    }

    result.x *= 60.0f;
    if (result.x < 0.0)
    {
        result.x += 360.0f;
    }

    return result;
}

Image::Color Image::fromHsv(const Vec4f &hsv)
{
    Vec4f rgb;

    if (hsv.y <= 0.0f)
    {
        rgb = Vec4f(hsv.z, hsv.z, hsv.z, hsv.w);
    }
    else
    {
        float hh = hsv.x;
        if (hh >= 360.0f) hh = 0.0;
        hh /= 60.0;

        const float ff = fract(hh);
        const float p = hsv.z * (1.0f -  hsv.y);
        const float q = hsv.z * (1.0f - (hsv.y *         ff));
        const float t = hsv.z * (1.0f - (hsv.y * (1.0f - ff)));

        const int i = int(hh);
        if (i == 0)
        {
            rgb = Vec4f(hsv.z, t, p, hsv.w);
        }
        else if (i == 1)
        {
            rgb = Vec4f(q, hsv.z, p, hsv.w);
        }
        else if (i == 2)
        {
            rgb = Vec4f(p, hsv.z, t, hsv.w);
        }
        else if (i == 3)
        {
            rgb = Vec4f(p, q, hsv.z, hsv.w);
        }
        else if (i == 4)
        {
            rgb = Vec4f(t, p, hsv.z, hsv.w);
        }
        else
        {
            rgb = Vec4f(hsv.z, p, q, hsv.w);
        }
    }

    return (rgb.min(Vec4f(1)).max(Vec4f()) * 255.f + Vec4f(.5f, .5f, .5f, .5f)).toVec4ub();
}

Image::Color Image::mix(Color a, Color b, Color m)
{
    const duint mr = m.x;
    const duint mg = m.y;
    const duint mb = m.z;
    const duint ma = m.w;
    
    int red   = (b.x * mr + a.x * (255u - mr)) / 255u;
    int green = (b.y * mg + a.y * (255u - mg)) / 255u;
    int blue  = (b.z * mb + a.z * (255u - mb)) / 255u;
    int alpha = (b.w * ma + a.w * (255u - ma)) / 255u;
    
    return makeColor(red, green, blue, alpha);
}
    
} // namespace de
