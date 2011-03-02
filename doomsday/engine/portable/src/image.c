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

void GL_ConvertToLuminance(image_t* image)
{
    assert(image);
    {
    int p, total = image->width * image->height;
    uint8_t* alphaChannel = NULL;
    uint8_t* ptr = image->pixels;

    if(image->pixelSize < 3)
    {   // No need to convert anything.
        return;
    }

    // Do we need to relocate the alpha data?
    if(image->pixelSize == 4)
    {   // Yes. Take a copy.
        alphaChannel = malloc(total);
        ptr = image->pixels;
        for(p = 0; p < total; ++p, ptr += image->pixelSize)
            alphaChannel[p] = ptr[3];
    }

    // Average the RGB colors.
    ptr = image->pixels;
    for(p = 0; p < total; ++p, ptr += image->pixelSize)
    {
        int min = MIN_OF(ptr[0], MIN_OF(ptr[1], ptr[2]));
        int max = MAX_OF(ptr[0], MAX_OF(ptr[1], ptr[2]));
        image->pixels[p] = (min + max) / 2;
    }

    // Do we need to relocate the alpha data?
    if(alphaChannel)
    {
        memcpy(image->pixels + total, alphaChannel, total);
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
    GL_ConvertToLuminance(image);
    { int p, total = image->width * image->height;
    for(p = 0; p < total; ++p)
    {
        image->pixels[total + p] = image->pixels[p];
        if(makeWhite)
            image->pixels[p] = 255;
    }}
    image->pixelSize = 2;
}

boolean GL_ImageHasAlpha(const image_t* img)
{
    assert(img);
    if(img->pixelSize == 3)
        return false;

    if(img->pixelSize == 4)
    {
        long i, numpels = img->width * img->height;
        const uint8_t* in = img->pixels;
        boolean hasAlpha = false;

        for(i = 0; i < numpels; ++i, in += 4)
            if(in[3] < 255)
            {
                hasAlpha = true;
                break;
            }
        return hasAlpha;
    }
#if _DEBUG
    Con_Error("GL_ImageHasAlpha: Attempted with non-rgb(a) image.");
#endif
    return false;
}
