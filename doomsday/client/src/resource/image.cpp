/** @file image.cpp Image objects and relates routines. 
 * @ingroup gl
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include <cstring>

#include "de_base.h"
#include "de_console.h"
#include "de_filesys.h"
#include "m_misc.h"
#ifdef __CLIENT__
#  include "resource/pcx.h"
#  include "resource/tga.h"
#  include "gl/gl_tex.h"
#endif
#include <de/Log>
#ifdef __CLIENT__
#  include <de/NativePath>
#  include <QByteArray>
#  include <QImage>
#endif

#include "resource/image.h"

#ifndef DENG2_QT_4_7_OR_NEWER // older than 4.7?
#  define constBits bits
#endif

using namespace de;

#ifdef __CLIENT__

struct GraphicFileType
{
    /// Symbolic name of the resource type.
    String name;

    /// Known file extension.
    String ext;

    bool (*interpretFunc)(de::FileHandle &hndl, String filePath, image_t &img);

    char const *(*getLastErrorFunc)(); ///< Can be NULL.
};

static bool interpretPcx(de::FileHandle &hndl, String /*filePath*/, image_t &img)
{
    Image_Init(&img);
    img.pixels = PCX_Load(reinterpret_cast<filehandle_s *>(&hndl),
                          &img.size.width, &img.size.height, &img.pixelSize);
    return (0 != img.pixels);
}

static bool interpretJpg(de::FileHandle &hndl, String /*filePath*/, image_t &img)
{
    return Image_LoadFromFileWithFormat(&img, "JPG", reinterpret_cast<filehandle_s *>(&hndl));
}

static bool interpretPng(de::FileHandle &hndl, String /*filePath*/, image_t &img)
{
    /*
    Image_Init(&img);
    img.pixels = PNG_Load(reinterpret_cast<filehandle_s *>(&hndl),
                          &img.size.width, &img.size.height, &img.pixelSize);
    return (0 != img.pixels);
    */
    return Image_LoadFromFileWithFormat(&img, "PNG", reinterpret_cast<filehandle_s *>(&hndl));
}

static bool interpretTga(de::FileHandle &hndl, String /*filePath*/, image_t &img)
{
    Image_Init(&img);
    img.pixels = TGA_Load(reinterpret_cast<filehandle_s *>(&hndl),
                          &img.size.width, &img.size.height, &img.pixelSize);
    return (0 != img.pixels);
}

// Graphic resource types.
static GraphicFileType const graphicTypes[] = {
    { "PNG",    "png",      interpretPng, 0 },
    { "JPG",    "jpg",      interpretJpg, 0 }, // TODO: add alternate "jpeg" extension
    { "TGA",    "tga",      interpretTga, TGA_LastError },
    { "PCX",    "pcx",      interpretPcx, PCX_LastError },
    { "",       "",         0,            0 } // Terminate.
};

static GraphicFileType const *guessGraphicFileTypeFromFileName(String fileName)
{
    // The path must have an extension for this.
    String ext = fileName.fileNameExtension();
    if(!ext.isEmpty())
    {
        for(int i = 0; !graphicTypes[i].ext.isEmpty(); ++i)
        {
            GraphicFileType const &type = graphicTypes[i];
            if(!ext.compareWithoutCase(type.ext))
            {
                return &type;
            }
        }
    }
    return 0; // Unknown.
}

static void interpretGraphic(de::FileHandle &hndl, String filePath, image_t &img)
{
    // Firstly try the interpreter for the guessed resource types.
    GraphicFileType const *rtypeGuess = guessGraphicFileTypeFromFileName(filePath);
    if(rtypeGuess)
    {
        rtypeGuess->interpretFunc(hndl, filePath, img);
    }

    // If not yet interpreted - try each recognisable format in order.
    if(!img.pixels)
    {
        // Try each recognisable format instead.
        /// @todo Order here should be determined by the resource locator.
        for(int i = 0; !graphicTypes[i].name.isEmpty(); ++i)
        {
            GraphicFileType const *graphicType = &graphicTypes[i];

            // Already tried this?
            if(graphicType == rtypeGuess) continue;

            graphicTypes[i].interpretFunc(hndl, filePath, img);
            if(img.pixels) break;
        }
    }
}

/// @return  @c true if the file name in @a path ends with the "color key" suffix.
static inline bool isColorKeyed(String path)
{
    return path.fileNameWithoutExtension().endsWith("-ck", Qt::CaseInsensitive);
}

#endif // __CLIENT__

void Image_Init(image_t *img)
{
    DENG2_ASSERT(img);
    img->size.width = 0;
    img->size.height = 0;
    img->pixelSize = 0;
    img->flags = 0;
    img->paletteId = 0;
    img->pixels = 0;
}

void Image_Destroy(image_t *img)
{
    DENG2_ASSERT(img);
    if(!img->pixels) return;

    M_Free(img->pixels);
    img->pixels = 0;
}

de::Vector2i Image_Dimensions(image_t const *img)
{
    DENG2_ASSERT(img);
    return Vector2i(img->size.width, img->size.height);
}

String Image_Description(image_t const *img)
{
    DENG2_ASSERT(img);
    Vector2i dimensions(img->size.width, img->size.height);

    return String("Dimensions:%1 Flags:%2 %3:%4")
               .arg(dimensions.asText())
               .arg(img->flags)
               .arg(0 != img->paletteId? "ColorPalette" : "PixelSize")
               .arg(0 != img->paletteId? img->paletteId : img->pixelSize);
}

void Image_ConvertToLuminance(image_t *img, boolean retainAlpha)
{
    DENG_ASSERT(img);
    LOG_AS("GL_ConvertToLuminance");

    uint8_t *alphaChannel = 0, *ptr = 0;

    // Is this suitable?
    if(0 != img->paletteId || (img->pixelSize < 3 && (img->flags & IMGF_IS_MASKED)))
    {
#if _DEBUG
        LOG_WARNING("Attempt to convert paletted/masked image. I don't know this format!");
#endif
        return;
    }

    long numPels = img->size.width * img->size.height;

    // Do we need to relocate the alpha data?
    if(retainAlpha && img->pixelSize == 4)
    {
        // Yes. Take a copy.
        alphaChannel = reinterpret_cast<uint8_t *>(M_Malloc(numPels));

        ptr = img->pixels;
        for(long p = 0; p < numPels; ++p, ptr += img->pixelSize)
        {
            alphaChannel[p] = ptr[3];
        }
    }

    // Average the RGB colors.
    ptr = img->pixels;
    for(long p = 0; p < numPels; ++p, ptr += img->pixelSize)
    {
        int min = MIN_OF(ptr[0], MIN_OF(ptr[1], ptr[2]));
        int max = MAX_OF(ptr[0], MAX_OF(ptr[1], ptr[2]));
        img->pixels[p] = (min == max? min : (min + max) / 2);
    }

    // Do we need to relocate the alpha data?
    if(alphaChannel)
    {
        std::memcpy(img->pixels + numPels, alphaChannel, numPels);
        img->pixelSize = 2;
        M_Free(alphaChannel);
        return;
    }

    img->pixelSize = 1;
}

void Image_ConvertToAlpha(image_t *img, boolean makeWhite)
{
    DENG2_ASSERT(img);

    Image_ConvertToLuminance(img, true);

    long total = img->size.width * img->size.height;
    for(long p = 0; p < total; ++p)
    {
        img->pixels[total + p] = img->pixels[p];
        if(makeWhite) img->pixels[p] = 255;
    }
    img->pixelSize = 2;
}

boolean Image_HasAlpha(image_t const *img)
{
    DENG2_ASSERT(img);
    LOG_AS("Image_HasAlpha");

    if(0 != img->paletteId || (img->flags & IMGF_IS_MASKED))
    {
#if _DEBUG
        LOG_WARNING("Attempt to determine alpha for paletted/masked image. I don't know this format!");
#endif
        return false;
    }

    if(img->pixelSize == 3)
    {
        return false;
    }

    if(img->pixelSize == 4)
    {
        long const numpels = img->size.width * img->size.height;
        uint8_t const *in = img->pixels;
        for(long i = 0; i < numpels; ++i, in += 4)
        {
            if(in[3] < 255)
            {
                return true;
            }
        }
    }
    return false;
}

uint8_t *Image_LoadFromFile(image_t *img, filehandle_s *_file)
{
#ifdef __CLIENT__
    DENG2_ASSERT(img && _file);
    de::FileHandle &file = reinterpret_cast<de::FileHandle &>(*_file);
    LOG_AS("Image_LoadFromFile");

    String filePath = file.file().composePath();

    Image_Init(img);
    interpretGraphic(file, filePath, *img);

    // Still not interpreted?
    if(!img->pixels)
    {
        LOG_VERBOSE("\"%s\" unrecognized, trying fallback loader...")
            << NativePath(filePath).pretty();
        return NULL; // Not a recognised format. It may still be loadable, however.
    }

    // How about some color-keying?
    if(isColorKeyed(filePath))
    {
        uint8_t *out = ApplyColorKeying(img->pixels, img->size.width, img->size.height, img->pixelSize);
        if(out != img->pixels)
        {
            // Had to allocate a larger buffer, free the old and attach the new.
            M_Free(img->pixels);
            img->pixels = out;
        }

        // Color keying is done; now we have 4 bytes per pixel.
        img->pixelSize = 4;
    }

    // Any alpha pixels?
    if(Image_HasAlpha(img))
    {
        img->flags |= IMGF_IS_MASKED;
    }

    LOG_VERBOSE("\"%s\" (%ix%i)")
        << NativePath(filePath).pretty() << img->size.width << img->size.height;

    return img->pixels;
#else
    // Server does not load image files.
    DENG2_UNUSED2(img, _file);
    return NULL;
#endif
}

boolean Image_LoadFromFileWithFormat(image_t *img, char const *format, filehandle_s *_hndl)
{
#ifdef __CLIENT__
    /// @todo There are too many copies made here. It would be best if image_t
    /// contained an instance of QImage. -jk

    DENG2_ASSERT(img);
    DENG2_ASSERT(_hndl);
    de::FileHandle &hndl = *reinterpret_cast<de::FileHandle *>(_hndl);

    // It is assumed that file's position stays the same (could be trying multiple interpreters).
    size_t initPos = hndl.tell();

    Image_Init(img);

    // Load the file contents to a memory buffer.
    QByteArray data;
    data.resize(hndl.length() - initPos);
    hndl.read(reinterpret_cast<uint8_t*>(data.data()), data.size());

    QImage image = QImage::fromData(data, format);
    if(image.isNull())
    {
        // Back to the original file position.
        hndl.seek(initPos, SeekSet);
        return false;
    }

    //LOG_TRACE("Loading \"%s\"...") << NativePath(hndl->file().composePath()).pretty();

    // Convert paletted images to RGB.
    if(image.colorCount())
    {
        image = image.convertToFormat(QImage::Format_ARGB32);
        DENG_ASSERT(!image.colorCount());
        DENG_ASSERT(image.depth() == 32);
    }

    // Swap the red and blue channels for GL.
    image = image.rgbSwapped();

    img->size.width  = image.width();
    img->size.height = image.height();
    img->pixelSize   = image.depth() / 8;

    LOG_TRACE("Image_Load: size %i x %i depth %i alpha %b bytes %i")
        << img->size.width << img->size.height << img->pixelSize
        << image.hasAlphaChannel() << image.byteCount();

    img->pixels = reinterpret_cast<uint8_t *>(M_MemDup(image.constBits(), image.byteCount()));

    // Back to the original file position.
    hndl.seek(initPos, SeekSet);
    return true;
#else
    // Server does not load image files.
    DENG2_UNUSED3(img, format, _hndl);
    return false;
#endif
}

boolean Image_Save(image_t const *img, char const *filePath)
{
#ifdef __CLIENT__
    DENG2_ASSERT(img);

    // Compose the full path.
    String fullPath = String(filePath);
    if(fullPath.isEmpty())
    {
        static int n = 0;
        fullPath = String("image%1x%2-%3").arg(img->size.width).arg(img->size.height).arg(n++, 3);
    }

    if(fullPath.fileNameExtension().isEmpty())
    {
        fullPath += ".png";
    }

    // Swap red and blue channels then save.
    QImage image = QImage(img->pixels, img->size.width, img->size.height, QImage::Format_ARGB32);
    image = image.rgbSwapped();

    return CPP_BOOL(image.save(NativePath(fullPath)));
#else
    // Server does not save images.
    DENG2_UNUSED2(img, filePath);
    return false;
#endif
}
