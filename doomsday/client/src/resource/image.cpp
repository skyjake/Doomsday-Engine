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

#include "de_base.h"
#include "de_console.h"
#include "de_filesys.h"
#include "m_misc.h"
#include "resource/image.h"

#include <de/Log>
#include <de/NativePath>
#include <QByteArray>
#include <QImage>

#ifndef DENG2_QT_4_7_OR_NEWER // older than 4.7?
#  define constBits bits
#endif

using namespace de;

void Image_Init(image_t *img)
{
    DENG_ASSERT(img);
    img->size.width = 0;
    img->size.height = 0;
    img->pixelSize = 0;
    img->flags = 0;
    img->paletteId = 0;
    img->pixels = 0;
}

void Image_Destroy(image_t *img)
{
    DENG_ASSERT(img);
    if(!img->pixels) return;

    M_Free(img->pixels);
    img->pixels = 0;
}

void Image_PrintMetadata(image_t const *image)
{
    DENG_ASSERT(image);
    Con_Printf("dimensions:[%ix%i] flags:%i %s:%i\n", image->size.width, image->size.height,
               image->flags, 0 != image->paletteId? "colorpalette" : "pixelsize",
               0 != image->paletteId? image->paletteId : image->pixelSize);
}

void Image_ConvertToLuminance(image_t *image, boolean retainAlpha)
{
    DENG_ASSERT(image);
    LOG_AS("GL_ConvertToLuminance");

    uint8_t* alphaChannel = 0, *ptr = 0;
    long p, numPels;

    // Is this suitable?
    if(0 != image->paletteId || (image->pixelSize < 3 && (image->flags & IMGF_IS_MASKED)))
    {
#if _DEBUG
        LOG_WARNING("Attempt to convert paletted/masked image. I don't know this format!");
#endif
        return;
    }

    numPels = image->size.width * image->size.height;

    // Do we need to relocate the alpha data?
    if(retainAlpha && image->pixelSize == 4)
    {
        // Yes. Take a copy.
        alphaChannel = reinterpret_cast<uint8_t*>(malloc(numPels));
        if(!alphaChannel) Con_Error("GL_ConvertToLuminance: Failed on allocation of %lu bytes for pixel alpha relocation buffer.", (unsigned long) numPels);

        ptr = image->pixels;
        for(p = 0; p < numPels; ++p, ptr += image->pixelSize)
        {
            alphaChannel[p] = ptr[3];
        }
    }

    // Average the RGB colors.
    ptr = image->pixels;
    for(p = 0; p < numPels; ++p, ptr += image->pixelSize)
    {
        int min = MIN_OF(ptr[0], MIN_OF(ptr[1], ptr[2]));
        int max = MAX_OF(ptr[0], MAX_OF(ptr[1], ptr[2]));
        image->pixels[p] = (min == max? min : (min + max) / 2);
    }

    // Do we need to relocate the alpha data?
    if(alphaChannel)
    {
        memcpy(image->pixels + numPels, alphaChannel, numPels);
        image->pixelSize = 2;
        M_Free(alphaChannel);
        return;
    }

    image->pixelSize = 1;
}

void Image_ConvertToAlpha(image_t *image, boolean makeWhite)
{
    DENG_ASSERT(image);

    Image_ConvertToLuminance(image, true);

    long total = image->size.width * image->size.height;
    for(long p = 0; p < total; ++p)
    {
        image->pixels[total + p] = image->pixels[p];
        if(makeWhite) image->pixels[p] = 255;
    }
    image->pixelSize = 2;
}

boolean Image_HasAlpha(image_t const *image)
{
    DENG_ASSERT(image);
    LOG_AS("Image_HasAlpha");

    if(0 != image->paletteId || (image->flags & IMGF_IS_MASKED))
    {
#if _DEBUG
        LOG_WARNING("Attempt to determine alpha for paletted/masked image. I don't know this format!");
#endif
        return false;
    }

    if(image->pixelSize == 3)
    {
        return false;
    }

    if(image->pixelSize == 4)
    {
        long const numpels = image->size.width * image->size.height;
        uint8_t const *in = image->pixels;
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

boolean Image_LoadFromFileWithFormat(image_t *img, char const *format, filehandle_s *_hndl)
{
    /// @todo There are too many copies made here. It would be best if image_t
    /// contained an instance of QImage. -jk

    DENG_ASSERT(img);
    DENG_ASSERT(_hndl);
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
}

boolean Image_Save(image_t const *img, char const *filePath)
{
    DENG_ASSERT(img);

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
}
