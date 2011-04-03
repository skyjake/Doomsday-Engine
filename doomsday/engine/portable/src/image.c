/**\file r_lumobjs.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include "de_base.h"
#include "de_console.h"

#include "image.h"

void GL_ConvertToLuminance(image_t* image, boolean retainAlpha)
{
    assert(image);
    {
    long p, numPels = image->width * image->height;
    uint8_t* alphaChannel = NULL;
    uint8_t* ptr = image->pixels;

    if(0 != image->palette || (image->flags & IMGF_IS_MASKED))
    {
#if _DEBUG
        Con_Message("Warning:GL_ConvertToLuminance: Attempt to convert "
            "paletted/masked image. I don't know this format!");
#endif
        return;
    }

    if(image->pixelSize < 3)
        return; // No conversion necessary.

    // Do we need to relocate the alpha data?
    if(retainAlpha && image->pixelSize == 4)
    {   // Yes. Take a copy.
        if(NULL == (alphaChannel = malloc(numPels)))
            Con_Error("GL_ConvertToLuminance: Failed on allocation of %lu bytes for "
                "pixel alpha relocation buffer.", (unsigned int) numPels);
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
}

void GL_ConvertToAlpha(image_t* image, boolean makeWhite)
{
    assert(image);
    GL_ConvertToLuminance(image, true);
    { long p, total = image->width * image->height;
    for(p = 0; p < total; ++p)
    {
        image->pixels[total + p] = image->pixels[p];
        if(makeWhite)
            image->pixels[p] = 255;
    }}
    image->pixelSize = 2;
}

boolean GL_ImageHasAlpha(const image_t* image)
{
    assert(image);

    if(0 != image->palette || (image->flags & IMGF_IS_MASKED))
    {
#if _DEBUG
        Con_Message("Warning:GL_ImageHasAlpha: Attempt to determine alpha for "
            "paletted/masked image. I don't know this format!");
#endif
        return false;
    }

    if(image->pixelSize == 3)
        return false;

    if(image->pixelSize == 4)
    {
        long i, numpels = image->width * image->height;
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
