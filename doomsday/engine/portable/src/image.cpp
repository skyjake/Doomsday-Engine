/**
 * @file image.cpp
 * Image objects and relates routines. @ingroup gl
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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
#include "m_misc.h"
#include "image.h"

#include <de/Log>
#include <QByteArray>
#include <QImage>

#if (QT_VERSION < QT_VERSION_CHECK(4, 7, 0))
#  define constBits bits
#endif

void Image_ConvertToLuminance(image_t* image, boolean retainAlpha)
{
    uint8_t* alphaChannel = NULL, *ptr = NULL;
    long p, numPels;
    assert(image);

    // Is this suitable?
    if(0 != image->paletteId || (image->pixelSize < 3 && (image->flags & IMGF_IS_MASKED)))
    {
#if _DEBUG
        Con_Message("Warning:GL_ConvertToLuminance: Attempt to convert "
                    "paletted/masked image. I don't know this format!");
#endif
        return;
    }

    numPels = image->size.width * image->size.height;

    // Do we need to relocate the alpha data?
    if(retainAlpha && image->pixelSize == 4)
    {
        // Yes. Take a copy.
        alphaChannel = reinterpret_cast<uint8_t*>(malloc(numPels));
        if(!alphaChannel)
            Con_Error("GL_ConvertToLuminance: Failed on allocation of %lu bytes for pixel alpha relocation buffer.", (unsigned long) numPels);
        ptr = image->pixels;
        for(p = 0; p < numPels; ++p, ptr += image->pixelSize)
            alphaChannel[p] = ptr[3];
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
        free(alphaChannel);
        return;
    }

    image->pixelSize = 1;
}

void Image_ConvertToAlpha(image_t* image, boolean makeWhite)
{
    long p, total;
    assert(image);
    Image_ConvertToLuminance(image, true);

    total = image->size.width * image->size.height;
    for(p = 0; p < total; ++p)
    {
        image->pixels[total + p] = image->pixels[p];
        if(makeWhite)
            image->pixels[p] = 255;
    }
    image->pixelSize = 2;
}

boolean Image_HasAlpha(const image_t* image)
{
    assert(image);

    if(0 != image->paletteId || (image->flags & IMGF_IS_MASKED))
    {
#if _DEBUG
        Con_Message("Warning:Image_HasAlpha: Attempt to determine alpha for "
            "paletted/masked image. I don't know this format!");
#endif
        return false;
    }

    if(image->pixelSize == 3)
        return false;

    if(image->pixelSize == 4)
    {
        long i, numpels = image->size.width * image->size.height;
        const uint8_t* in = image->pixels;
        boolean hasAlpha = false;
        for(i = 0; i < numpels; ++i, in += 4)
            if(in[3] < 255)
            {
                hasAlpha = true;
                break;
            }
        return hasAlpha;
    }
    return false;
}

boolean Image_LoadFromFileWithFormat(image_t* img, const char* format, DFile* file)
{
    /// @todo There are too many copies made here. It would be best if image_t
    /// contained an instance of QImage. -jk

    assert(img);
    assert(file);

    // It is assumed that file's position stays the same (could be trying multiple loaders).
    size_t initPos = DFile_Tell(file);

    GL_InitImage(img);

    // Load the file contents to a memory buffer.
    QByteArray data;
    data.resize(DFile_Length(file) - initPos);
    DFile_Read(file, reinterpret_cast<uint8_t*>(data.data()), data.size());

    QImage image = QImage::fromData(data, format);
    if(image.isNull())
    {
        // Back to the original file position.
        DFile_Seek(file, initPos, SEEK_SET);
        return false;
    }

    // Convert paletted images to RGB.
    if(image.colorCount() && !image.hasAlphaChannel())
    {
        image = image.convertToFormat(QImage::Format_RGB888);
        assert(!image.colorCount());
        assert(image.depth() == 24);
    }
    else if(image.colorCount() && image.hasAlphaChannel())
    {
        image = image.convertToFormat(QImage::Format_ARGB32);
        assert(image.depth() == 32);
    }
    else
    {
        // Swap the red and blue channels.
        image = image.rgbSwapped();
    }

    img->size.width  = image.width();
    img->size.height = image.height();
    img->pixelSize   = image.depth() / 8;

    LOG_TRACE("Image_Load: size %i x %i depth %i alpha %b bytes %i")
              << img->size.width << img->size.height << img->pixelSize
              << image.hasAlphaChannel() << image.byteCount();

    img->pixels = reinterpret_cast<uint8_t*>(M_MemDup(image.constBits(), image.byteCount()));

    // Back to the original file position.
    DFile_Seek(file, initPos, SEEK_SET);
    return true;
}
