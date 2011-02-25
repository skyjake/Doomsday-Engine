/**\file gl_tex.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
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

/**
 * Image manipulation algorithms.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <math.h>
#include <ctype.h>

#include "de_base.h"
#include "de_console.h"
#include "de_refresh.h"
#include "de_graphics.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static byte *scratchBuffer = NULL;
static size_t scratchBufferSize = 0;

// CODE --------------------------------------------------------------------

/**
 * Provides a persistent scratch buffer for use by texture manipulation
 * routines e.g. scaleLine().
 */
static byte *GetScratchBuffer(size_t size)
{
    // Need to enlarge?
    if(size > scratchBufferSize)
    {
        scratchBuffer = Z_Realloc(scratchBuffer, size, PU_APPSTATIC);
        scratchBufferSize = size;
    }

    return scratchBuffer;
}

/**
 * Copies a rectangular region of the source buffer to the destination
 * buffer. Doesn't perform clipping, so be careful.
 * Yeah, 13 parameters...
 */
void pixBlt(byte *src, int srcWidth, int srcHeight, byte *dest, int destWidth,
            int destHeight, int alpha, int srcRegX, int srcRegY, int destRegX,
            int destRegY, int regWidth, int regHeight)
{
    int         y;                  // Y in the copy region.
    int         srcNumPels = srcWidth * srcHeight;
    int         destNumPels = destWidth * destHeight;

    for(y = 0; y < regHeight; ++y)  // Copy line by line.
    {
        // The color index data.
        memcpy(dest + destRegX + (y + destRegY) * destWidth,
               src + srcRegX + (y + srcRegY) * srcWidth, regWidth);

        if(alpha)
        {
            // Alpha channel data.
            memcpy(dest + destNumPels + destRegX + (y + destRegY) * destWidth,
                   src + srcNumPels + srcRegX + (y + srcRegY) * srcWidth,
                   regWidth);
        }
    }
}

/**
 * in/out format:
 * 1 = palette indices
 * 2 = palette indices followed by alpha values
 * 3 = RGB
 * 4 = RGBA
 */
void GL_ConvertBuffer(int width, int height, int informat, int outformat,
                      byte *in, byte *out, colorpaletteid_t palid,
                      boolean gamma)
{
    if(informat == outformat)
    {
        // No conversion necessary.
        memcpy(out, in, width * height * informat);
        return;
    }

    // Conversion from pal8(a) to RGB(A).
    if(informat <= 2 && outformat >= 3)
    {
        GL_PalettizeImage(out, outformat, R_GetColorPalette(palid), gamma,
                          in, informat, width, height);
    }
    // Conversion from RGB(A) to pal8(a), using pal18To8.
    else if(informat >= 3 && outformat <= 2)
    {
        GL_QuantizeImageToPalette(out, outformat, R_GetColorPalette(palid),
                                  in, informat, width, height);
    }
    else if(informat == 3 && outformat == 4)
    {
        int                 i, numPixels = width * height,
                            inSize = (informat == 2 ? 1 : informat),
                            outSize = (outformat == 2 ? 1 : outformat);

        for(i = 0; i < numPixels; ++i, in += inSize, out += outSize)
        {
            memcpy(out, in, 3);
            out[3] = 0xff;      // Opaque.
        }
    }
}

/**
 * Len is measured in out units. Comps is the number of components per
 * pixel, or rather the number of bytes per pixel (3 or 4). The strides must
 * be byte-aligned anyway, though; not in pixels.
 *
 * \fixme Probably could be optimized.
 */
static void scaleLine(byte *in, int inStride, byte *out, int outStride,
                      int outLen, int inLen, int comps)
{
    int         i, c;
    float       inToOutScale = outLen / (float) inLen;

    if(inToOutScale > 1)
    {
        // Magnification is done using linear interpolation.
        fixed_t     inPosDelta = (FRACUNIT * (inLen - 1)) / (outLen - 1);
        fixed_t     inPos = inPosDelta;
        byte       *col1, *col2;
        int         weight, invWeight;

        // The first pixel.
        memcpy(out, in, comps);
        out += outStride;

        // Step at each out pixel between the first and last ones.
        for(i = 1; i < outLen - 1; ++i, out += outStride, inPos += inPosDelta)
        {
            col1 = in + (inPos >> FRACBITS) * inStride;
            col2 = col1 + inStride;
            weight = inPos & 0xffff;
            invWeight = 0x10000 - weight;

            out[0] = (byte)((col1[0] * invWeight + col2[0] * weight) >> 16);
            out[1] = (byte)((col1[1] * invWeight + col2[1] * weight) >> 16);
            out[2] = (byte)((col1[2] * invWeight + col2[2] * weight) >> 16);
            if(comps == 4)
                out[3] = (byte)((col1[3] * invWeight + col2[3] * weight) >> 16);
        }

        // The last pixel.
        memcpy(out, in + (inLen - 1) * inStride, comps);
    }
    else if(inToOutScale < 1)
    {
        // Minification needs to calculate the average of each of
        // the pixels contained by the out pixel.
        uint        cumul[4] = { 0, 0, 0, 0 }, count = 0;
        int         outpos = 0;

        for(i = 0; i < inLen; ++i, in += inStride)
        {
            if((int) (i * inToOutScale) != outpos)
            {
                outpos = (int) (i * inToOutScale);

                for(c = 0; c < comps; ++c)
                {
                    out[c] = (byte)(cumul[c] / count);
                    cumul[c] = 0;
                }
                count = 0;
                out += outStride;
            }
            for(c = 0; c < comps; ++c)
                cumul[c] += in[c];
            count++;
        }
        // Fill in the last pixel, too.
        if(count)
            for(c = 0; c < comps; ++c)
                out[c] = (byte)(cumul[c] / count);
    }
    else
    {
        // No need for scaling.
        if(comps == 3)
        {
            for(i = outLen; i > 0; --i, out += outStride, in += inStride)
            {
                out[0] = in[0];
                out[1] = in[1];
                out[2] = in[2];
            }
        }
        else if(comps == 4)
        {
            for(i = outLen; i > 0; i--, out += outStride, in += inStride)
            {
                out[0] = in[0];
                out[1] = in[1];
                out[2] = in[2];
                out[3] = in[3];
            }
        }
    }
}

void GL_ScaleBuffer32(byte *in, int inWidth, int inHeight, byte *out,
                      int outWidth, int outHeight, int comps)
{
    int         i;
    int         stride;
    uint        inOffsetSize, outOffsetSize;
    byte       *inOff, *outOff;
    byte       *buffer;

    buffer = GetScratchBuffer(outWidth * inHeight * comps);

    // First scale horizontally, to outWidth, into the temporary buffer.
    inOff = in;
    outOff = buffer;
    inOffsetSize = inWidth * comps;
    outOffsetSize = outWidth * comps;
    for(i = 0; i < inHeight;
        ++i, inOff += inOffsetSize, outOff += outOffsetSize)
    {
        scaleLine(inOff, comps, outOff, comps, outWidth, inWidth, comps);
    }

    // Then scale vertically, to outHeight, into the out buffer.
    inOff = buffer;
    outOff = out;
    stride = outWidth * comps;
    inOffsetSize = comps;
    outOffsetSize = comps;
    for(i = 0; i < outWidth;
        ++i, inOff += inOffsetSize, outOff += outOffsetSize)
    {
        scaleLine(inOff, stride, outOff, stride, outHeight, inHeight,
                  comps);
    }
}

/**
 * Works within the given data, reducing the size of the picture to half
 * its original. Width and height must be powers of two.
 */
void GL_DownMipmap32(byte *in, int width, int height, int comps)
{
    byte       *out;
    int         x, y, c, outW = width >> 1, outH = height >> 1;

    if(width == 1 && height == 1)
    {
#if _DEBUG
        Con_Error("GL_DownMipmap32 can't be called for a 1x1 image.\n");
#endif
        return;
    }

    if(!outW || !outH)          // Limited, 1x2|2x1 -> 1x1 reduction?
    {
        int     outDim = (width > 1 ? outW : outH);

        out = in;
        for(x = 0; x < outDim; ++x, in += comps * 2)
            for(c = 0; c < comps; ++c, out++)
                *out = (byte)((in[c] + in[comps + c]) >> 1);
    }
    else                        // Unconstrained, 2x2 -> 1x1 reduction?
    {
        out = in;
        for(y = 0; y < outH; ++y, in += width * comps)
            for(x = 0; x < outW; ++x, in += comps * 2)
                for(c = 0; c < comps; ++c, out++)
                    *out =
                        (byte)((in[c] + in[comps + c] + in[comps * width + c] +
                               in[comps * (width + 1) + c]) >> 2);
    }
}

/**
 * Determine the optimal size for a texture. Usually the dimensions are
 * scaled upwards to the next power of two.
 *
 * @param noStretch         If @c true, the stretching can be skipped.
 * @param isMipMapped       If @c true, we will require mipmaps (this has
 *                          an affect on the optimal size).
 */
boolean GL_OptimalSize(int width, int height, int *optWidth, int *optHeight,
                       boolean noStretch, boolean isMipMapped)
{
    if(GL_state.textureNonPow2 && !isMipMapped)
    {
        *optWidth = width;
        *optHeight = height;
    }
    else if(noStretch)
    {
        *optWidth = M_CeilPow2(width);
        *optHeight = M_CeilPow2(height);

        // MaxTexSize may prevent using noStretch.
        if(*optWidth  > GL_state.maxTexSize ||
           *optHeight > GL_state.maxTexSize)
        {
            noStretch = false;
        }
    }
    else
    {
        // Determine the most favorable size for the texture.
        if(texQuality == TEXQ_BEST) // The best quality.
        {
            // At the best texture quality *opt, all textures are
            // sized *upwards*, so no details are lost. This takes
            // more memory, but naturally looks better.
            *optWidth = M_CeilPow2(width);
            *optHeight = M_CeilPow2(height);
        }
        else if(texQuality == 0)
        {
            // At the lowest quality, all textures are sized down
            // to the nearest power of 2.
            *optWidth = M_FloorPow2(width);
            *optHeight = M_FloorPow2(height);
        }
        else
        {
            // At the other quality *opts, a weighted rounding
            // is used.
            *optWidth = M_WeightPow2(width, 1 - texQuality / (float) TEXQ_BEST);
            *optHeight =
               M_WeightPow2(height, 1 - texQuality / (float) TEXQ_BEST);
        }
    }

    // Hardware limitations may force us to modify the preferred
    // texture size.
    if(*optWidth > GL_state.maxTexSize)
        *optWidth = GL_state.maxTexSize;
    if(*optHeight > GL_state.maxTexSize)
        *optHeight = GL_state.maxTexSize;

    // Some GL drivers seem to have problems with VERY small textures.
    if(*optWidth < MINTEXWIDTH)
        *optWidth = MINTEXWIDTH;
    if(*optHeight < MINTEXHEIGHT)
        *optHeight = MINTEXHEIGHT;

    if(ratioLimit)
    {
        if(*optWidth > *optHeight) // Wide texture.
        {
            if(*optHeight < *optWidth / ratioLimit)
                *optHeight = *optWidth / ratioLimit;
        }
        else // Tall texture.
        {
            if(*optWidth < *optHeight / ratioLimit)
                *optWidth = *optHeight / ratioLimit;
        }
    }

    return noStretch;
}

/**
 * Converts the image data to grayscale luminance in-place.
 */
void GL_ConvertToLuminance(image_t* image)
{
    int p, total = image->width * image->height;
    byte* alphaChannel = NULL;
    byte* ptr = image->pixels;

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

void GL_ConvertToAlpha(image_t *image, boolean makeWhite)
{
    int         p, total = image->width * image->height;

    GL_ConvertToLuminance(image);
    for(p = 0; p < total; ++p)
    {
        // Move the average color to the alpha channel, make the
        // actual color white.
        image->pixels[total + p] = image->pixels[p];
        if(makeWhite)
            image->pixels[p] = 255;
    }

    image->pixelSize = 2;
}

boolean ImageHasAlpha(image_t *img)
{
    uint        i, size;
    boolean     hasAlpha = false;
    byte       *in;

    if(img->pixelSize == 4)
    {
        size = img->width * img->height;
        for(i = 0, in = img->pixels; i < size; ++i, in += 4)
            if(in[3] < 255)
            {
                hasAlpha = true;
                break;
            }
    }

    return hasAlpha;
}

int LineAverageRGB(byte *imgdata, int width, int height, int line,
                   byte *rgb, byte *palette, boolean hasAlpha)
{
    byte       *start = imgdata + width * line;
    byte       *alphaStart = start + width * height;
    int         i, c, count = 0;
    int         integerRGB[3] = { 0, 0, 0 };
    byte        col[3];

    for(i = 0; i < width; ++i)
    {
        // Not transparent?
        if(alphaStart[i] > 0 || !hasAlpha)
        {
            count++;
            // Ignore the gamma level.
            memcpy(col, palette + 3 * start[i], 3);
            for(c = 0; c < 3; ++c)
                integerRGB[c] += col[c];
        }
    }
    // All transparent? Sorry...
    if(!count)
        return 0;

    // We're going to make it!
    for(c = 0; c < 3; ++c)
        rgb[c] = (byte) (integerRGB[c] / count);
    return 1; // Successful.
}

/**
 * Fills the empty pixels with reasonable color indices in order to get rid
 * of black outlines caused by texture filtering.
 *
 * \fixme Not a very efficient algorithm...
 */
void ColorOutlines(byte *buffer, int width, int height)
{
    int         i, k, a, b;
    byte       *ptr;
    const int   numpels = width * height;

    for(k = 0; k < height; ++k)
        for(i = 0; i < width; ++i)
            // Solid pixels spread around...
            if(buffer[numpels + i + k * width])
            {
                for(b = -1; b <= 1; ++b)
                    for(a = -1; a <= 1; ++a)
                    {
                        // First check that the pixel is OK.
                        if((!a && !b) || i + a < 0 || k + b < 0 ||
                           i + a >= width || k + b >= height)
                            continue;

                        ptr = buffer + i + a + (k + b) * width;
                        if(!ptr[numpels])   // An alpha pixel?
                            *ptr = buffer[i + k * width];
                    }
            }
}

/**
 * The given RGB color is scaled uniformly so that the highest component
 * becomes one.
 */
void amplify(float* rgb)
{
    float max = 0;
    int i;

    for(i = 0; i < 3; ++i)
        if(rgb[i] > max)
            max = rgb[i];

    if(!max || max == 1)
        return;

    if(max)
    {
        for(i = 0; i < 3; ++i)
            rgb[i] = rgb[i] / max;
    }
}

/**
 * Used by flares and dynamic lights.
 */
void averageColorIdx(rgbcol_t col, byte* data, int w, int h,
    colorpaletteid_t palid, boolean hasAlpha)
{
    const int numpels = w * h;
    byte* alphaStart = data + numpels;
    DGLubyte rgbUBV[3];
    float r, g, b;
    DGLuint pal = R_GetColorPalette(palid);
    uint count;
    int i;

    // First clear them.
    for(i = 0; i < 3; ++i)
        col[i] = 0;

    r = g = b = count = 0;
    for(i = 0; i < numpels; ++i)
    {
        if(!hasAlpha || alphaStart[i])
        {
            count++;

            GL_GetColorPaletteRGB(pal, rgbUBV, data[i]);

            // Ignore the gamma level.
            r += rgbUBV[CR] / 255.f;
            g += rgbUBV[CG] / 255.f;
            b += rgbUBV[CB] / 255.f;
        }
    }
    // All transparent? Sorry...
    if(!count)
        return;

    col[0] = r / count;
    col[1] = g / count;
    col[2] = b / count;
}

int lineAverageColorIdx(rgbcol_t col, byte* data, int w, int h, int line,
                        colorpaletteid_t palid, boolean hasAlpha)
{
    int                 i;
    uint                count;
    const int           numpels = w * h;
    byte*               start = data + w * line;
    byte*               alphaStart = data + numpels + w * line;
    DGLubyte            rgbUBV[3];
    float               r, g, b;
    DGLuint             pal = R_GetColorPalette(palid);

    // First clear them.
    for(i = 0; i < 3; ++i)
        col[i] = 0;

    r = g = b = count = 0;
    for(i = 0; i < w; ++i)
    {
        if(!hasAlpha || alphaStart[i])
        {
            count++;

            GL_GetColorPaletteRGB(pal, rgbUBV, start[i]);

            // Ignore the gamma level.
            r += rgbUBV[CR] / 255.f;
            g += rgbUBV[CG] / 255.f;
            b += rgbUBV[CB] / 255.f;
        }
    }
    // All transparent? Sorry...
    if(!count)
        return 0;

    col[0] = r / count;
    col[1] = g / count;
    col[2] = b / count;

    return 1; // Successful.
}

int lineAverageColorRGB(rgbcol_t col, byte* data, int w, int h, int line)
{
    int             i;
    float           culmul[3];

    for(i = 0; i < 3; ++i)
        culmul[i] = 0;

    *data += 3 * w * line;
    for(i = 0; i < w; ++i)
    {
        culmul[CR] += (*data++) / 255.f;
        culmul[CG] += (*data++) / 255.f;
        culmul[CB] += (*data++) / 255.f;
    }

    col[CR] = culmul[CR] / w;
    col[CG] = culmul[CG] / w;
    col[CB] = culmul[CB] / w;

    return 1; // Successful.
}

void averageColorRGB(rgbcol_t col, byte *data, int w, int h)
{
    uint        i;
    const uint  numpels = w * h;
    float       cumul[3];

    if(!numpels)
        return;

    for(i = 0; i < 3; ++i)
        cumul[i] = 0;
    for(i = 0; i < numpels; ++i)
    {
        cumul[0] += (*data++) / 255.f;
        cumul[1] += (*data++) / 255.f;
        cumul[2] += (*data++) / 255.f;
    }

    for(i = 0; i < 3; ++i)
        col[i] = cumul[i] / numpels;
}

/**
 * Calculates a clip region for the buffer that excludes alpha pixels.
 * NOTE: Cross spread from bottom > top, right > left (inside out).
 *
 * @param   buffer      Image data to be processed.
 * @param   width       Width of the src buffer.
 * @param   height      Height of the src buffer.
 * @param   pixelsize   Pixelsize of the src buffer. Handles 1 (==2), 3 and 4.
 *
 * @returnval region    Ptr to int[4] to write the resultant region to.
 */
void GL_GetNonAlphaRegion(byte *buffer, int width, int height, int pixelsize,
                       int *region)
{
    int         k, i;
    int         myregion[4];
    byte       *src = buffer;
    byte       *alphasrc = NULL;

    myregion[0] = width;
    myregion[1] = 0;
    myregion[2] = height;
    myregion[3] = 0;

    if(pixelsize == 1)
        // In paletted mode, the alpha channel follows the actual image.
        alphasrc = buffer + width * height;

    // \todo This is not very efficent. Better to use an algorithm which works on full rows and full columns.
    for(k = 0; k < height; ++k)
        for(i = 0; i < width; ++i, src += pixelsize, alphasrc++)
        {
            // Alpha pixels don't count.
            if(pixelsize == 1)
            {
                if(*alphasrc < 255)
                    continue;
            }
            else if(pixelsize == 4)
            {
                if(src[3] < 255)
                    continue;
            }

            if(i < myregion[0])
                myregion[0] = i;
            if(i > myregion[1])
                myregion[1] = i;

            if(k < myregion[2])
                myregion[2] = k;
            if(k > myregion[3])
                myregion[3] = k;
        }

    memcpy(region, myregion, sizeof(myregion));
}

/**
 * Calculates the properties of a dynamic light that the given sprite frame
 * casts. Crop a boundary around the image to remove excess alpha'd pixels
 * from adversely affecting the calculation.
 * Handles pixel sizes; 1 (==2), 3 and 4.
 */
void GL_CalcLuminance(byte* buffer, int width, int height, int pixelSize,
                      colorpaletteid_t palid, float* brightX,
                      float* brightY, rgbcol_t* color, float* lumSize)
{
    DGLuint             pal =
        (pixelSize == 1? R_GetColorPalette(palid) : 0);
    int                 i, k, x, y, c, cnt = 0, posCnt = 0;
    byte                rgb[3], *src, *alphaSrc = NULL;
    int                 limit = 0xc0, posLimit = 0xe0, colLimit = 0xc0;
    int                 avgCnt = 0, lowCnt = 0;
    float               average[3], lowAvg[3];
    int                 region[4];

    for(i = 0; i < 3; ++i)
    {
        average[i] = 0;
        lowAvg[i] = 0;
    }
    src = buffer;

    if(pixelSize == 1)
    {
        // In paletted mode, the alpha channel follows the actual image.
        alphaSrc = buffer + width * height;
    }

    GL_GetNonAlphaRegion(buffer, width, height, pixelSize, &region[0]);
    if(region[2] > 0)
    {
        src += pixelSize * width * region[2];
        alphaSrc += width * region[2];
    }
    (*brightX) = (*brightY) = 0;

    for(k = region[2], y = 0; k < region[3] + 1; ++k, ++y)
    {
        if(region[0] > 0)
        {
            src += pixelSize * region[0];
            alphaSrc += region[0];
        }

        for(i = region[0], x = 0; i < region[1] + 1;
            ++i, ++x, src += pixelSize, alphaSrc++)
        {
            // Alpha pixels don't count.
            if(pixelSize == 1)
            {
                if(*alphaSrc < 255)
                    continue;
            }
            else if(pixelSize == 4)
            {
                if(src[3] < 255)
                    continue;
            }

            // Bright enough?
            if(pixelSize == 1)
            {
                GL_GetColorPaletteRGB(pal, (DGLubyte*) rgb, *src);
            }
            else if(pixelSize >= 3)
            {
                memcpy(rgb, src, 3);
            }

            if(rgb[0] > posLimit || rgb[1] > posLimit || rgb[2] > posLimit)
            {
                // This pixel will participate in calculating the average
                // center point.
                (*brightX) += x;
                (*brightY) += y;
                posCnt++;
            }

            // Bright enough to affect size?
            if(rgb[0] > limit || rgb[1] > limit || rgb[2] > limit)
                cnt++;

            // How about the color of the light?
            if(rgb[0] > colLimit || rgb[1] > colLimit || rgb[2] > colLimit)
            {
                avgCnt++;
                for(c = 0; c < 3; ++c)
                    average[c] += rgb[c] / 255.f;
            }
            else
            {
                lowCnt++;
                for(c = 0; c < 3; ++c)
                    lowAvg[c] += rgb[c] / 255.f;
            }
        }

        if(region[1] < width - 1)
        {
            src += pixelSize * (width - 1 - region[1]);
            alphaSrc += (width - 1 - region[1]);
        }
    }

    if(!posCnt)
    {
        (*brightX) = region[0] + ((region[1] - region[0]) / 2.0f);
        (*brightY) = region[2] + ((region[3] - region[2]) / 2.0f);
    }
    else
    {
        // Get the average.
        (*brightX) /= posCnt;
        (*brightY) /= posCnt;
        // Add the origin offset.
        (*brightX) += region[0];
        (*brightY) += region[2];
    }

    // Center on the middle of the brightest pixel.
    (*brightX) += .5f;
    (*brightY) += .5f;

    // The color.
    if(!avgCnt)
    {
        if(!lowCnt)
        {
            // Doesn't the thing have any pixels??? Use white light.
            for(c = 0; c < 3; ++c)
                (*color)[c] = 1;
        }
        else
        {
            // Low-intensity color average.
            for(c = 0; c < 3; ++c)
                (*color)[c] = lowAvg[c] / lowCnt;
        }
    }
    else
    {
        // High-intensity color average.
        for(c = 0; c < 3; ++c)
            (*color)[c] = average[c] / avgCnt;
    }

/*#ifdef _DEBUG
    Con_Message("GL_CalcLuminance: width %dpx, height %dpx, bits %d\n"
                "  cell region X[%d, %d] Y[%d, %d]\n"
                "  flare X= %g Y=%g %s\n"
                "  flare RGB[%g, %g, %g] %s\n",
                width, height, pixelSize,
                region[0], region[1], region[2], region[3],
                (*brightX), (*brightY),
                (posCnt? "(average)" : "(center)"),
                (*color)[0], (*color)[1], (*color)[2],
                (avgCnt? "(hi-intensity avg)" :
                 lowCnt? "(low-intensity avg)" : "(white light)"));
#endif*/

    // Amplify color.
    amplify(*color);

    // How about the size of the light source?
    *lumSize = MIN_OF(((2 * cnt + avgCnt) / 3.0f / 70.0f), 1);
}

/**
 * @return              @c true, if the given color is either
 *                      (0,255,255) or (255,0,255).
 */
static boolean ColorKey(byte *color)
{
    return color[CB] == 0xff && ((color[CR] == 0xff && color[CG] == 0) ||
                                 (color[CR] == 0 && color[CG] == 0xff));
}

/**
 * Buffer must be RGBA. Doesn't touch the non-keyed pixels.
 */
static void DoColorKeying(byte *rgbaBuf, unsigned int width)
{
    uint                i;

    for(i = 0; i < width; ++i, rgbaBuf += 4)
        if(ColorKey(rgbaBuf))
            rgbaBuf[3] = rgbaBuf[2] = rgbaBuf[1] = rgbaBuf[0] = 0;
}

/**
 * Take the input buffer and convert to color keyed. A new buffer may be
 * needed if the input buffer has three color components.
 *
 * @return              If the in buffer wasn't large enough will return a
 *                      ptr to the newly allocated buffer which must be
 *                      freed with M_Free().
 */
byte *GL_ApplyColorKeying(byte *buf, uint pixelSize, uint width,
                          uint height)
{
    uint                i;
    byte               *ckdest, *in, *out;
    const uint          numpels = width * height;

    // We must allocate a new buffer if the loaded image has three
    // color components.
    if(pixelSize < 4)
    {
        ckdest = M_Malloc(4 * width * height);
        for(in = buf, out = ckdest, i = 0; i < numpels;
            ++i, in += pixelSize, out += 4)
        {
            if(ColorKey(in))
                memset(out, 0, 4);  // Totally black.
            else
            {
                memcpy(out, in, 3); // The color itself.
                out[CA] = 255;  // Opaque.
            }
        }

        return ckdest;
    }
    else                    // We can do the keying in-buffer.
    {
        // This preserves the alpha values of non-keyed pixels.
        for(i = 0; i < height; ++i)
            DoColorKeying(buf + 4 * i * width, height);
    }

    return NULL;
}

void GL_ScaleBufferNearest(const byte* in, int width, int height,
    byte* out, int outWidth, int outHeight, int comps)
{
    int ratioX = (int)(width << 16 ) / outWidth + 1;
    int ratioY = (int)(height << 16) / outHeight + 1;
    int i, j;

    int shearY = 0;
    for(i = 0; i < outHeight; ++i, shearY += ratioY)
    {
        int shearX = 0;
        int shearY2 = (shearY >> 16) * width;
        for(j = 0; j < outWidth; ++j, out += comps, shearX += ratioX)
        {
            int c, n = (shearY2 + (shearX >> 16)) * comps;
            for(c = 0; c < comps; ++c, n++)
                out[c] = in[n];
        }
    }
}

int GL_PickSmartScaleMethod(int width, int height)
{
    if(width >= MINTEXWIDTH && height >= MINTEXHEIGHT)
        return 2; // hq2x
    return 1; // nearest neighbor.
}

void GL_SmartFilter(int method, byte* in, byte* out, int width, int height)
{
    switch(method)
    {
    default: // linear interpolation.
        GL_ScaleBuffer32(in, width, height, out, width*2, height*2, 4);
        break;
    case 1: // nearest neighbor.
        GL_ScaleBufferNearest(in, width, height, out, width*2, height*2, 4);
        break;
    case 2: // hq2x
        GL_SmartFilter2x(in, width, height, out);
        break;
    };
}
