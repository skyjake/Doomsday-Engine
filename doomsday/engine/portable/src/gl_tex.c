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

#include <stdlib.h>
#include <math.h>
#include <ctype.h>

#include "de_base.h"
#include "de_console.h"
#include "de_refresh.h"
//#include "de_graphics.h"
//#include "de_misc.h"

static uint8_t* scratchBuffer = NULL;
static size_t scratchBufferSize = 0;

/**
 * Provides a persistent scratch buffer for use by texture manipulation
 * routines e.g. scaleLine().
 */
static uint8_t* GetScratchBuffer(size_t size)
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
 * Len is measured in out units. Comps is the number of components per
 * pixel, or rather the number of bytes per pixel (3 or 4). The strides must
 * be byte-aligned anyway, though; not in pixels.
 *
 * \fixme Probably could be optimized.
 */
static void scaleLine(const uint8_t* in, int inStride, uint8_t* out, int outStride,
    int outLen, int inLen, int comps)
{
    float inToOutScale = outLen / (float) inLen;
    int i, c;

    if(inToOutScale > 1)
    {
        // Magnification is done using linear interpolation.
        fixed_t inPosDelta = (FRACUNIT * (inLen - 1)) / (outLen - 1);
        fixed_t inPos = inPosDelta;
        const uint8_t* col1, *col2;
        int weight, invWeight;

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

            out[0] = (uint8_t)((col1[0] * invWeight + col2[0] * weight) >> 16);
            out[1] = (uint8_t)((col1[1] * invWeight + col2[1] * weight) >> 16);
            out[2] = (uint8_t)((col1[2] * invWeight + col2[2] * weight) >> 16);
            if(comps == 4)
                out[3] = (uint8_t)((col1[3] * invWeight + col2[3] * weight) >> 16);
        }

        // The last pixel.
        memcpy(out, in + (inLen - 1) * inStride, comps);
        return;
    }

    if(inToOutScale < 1)
    {
        // Minification needs to calculate the average of each of
        // the pixels contained by the out pixel.
        uint cumul[4] = { 0, 0, 0, 0 }, count = 0;
        int outpos = 0;

        for(i = 0; i < inLen; ++i, in += inStride)
        {
            if((int) (i * inToOutScale) != outpos)
            {
                outpos = (int) (i * inToOutScale);

                for(c = 0; c < comps; ++c)
                {
                    out[c] = (uint8_t)(cumul[c] / count);
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
                out[c] = (uint8_t)(cumul[c] / count);
        return;
    }

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

/// \todo Avoid use of a secondary buffer by scaling directly to output.
uint8_t* GL_ScaleBuffer(const uint8_t* in, int width, int height, int comps,
    int outWidth, int outHeight)
{
    assert(in);
    {
    uint inOffsetSize, outOffsetSize;
    uint8_t* outOff, *buffer;
    const uint8_t* inOff;
    uint8_t* out;
    int stride;

    if(width <= 0 || height <= 0)
        return (uint8_t*)in;

    if(comps != 3 && comps != 4)
        Con_Error("GL_ScaleBuffer: Attempted on non-rgb(a) image (comps=%i).", comps);

    buffer = GetScratchBuffer(comps * outWidth * height);

    if(0 == (out = malloc(comps * outWidth * outHeight)))
        Con_Error("GL_ScaleBuffer: Failed on allocation of %lu bytes for "
                  "output buffer.", (unsigned long) (comps * outWidth * outHeight));

    // First scale horizontally, to outWidth, into the temporary buffer.
    inOff = in;
    outOff = buffer;
    inOffsetSize = width * comps;
    outOffsetSize = outWidth * comps;
    { int i;
    for(i = 0; i < height; ++i, inOff += inOffsetSize, outOff += outOffsetSize)
    {
        scaleLine(inOff, comps, outOff, comps, outWidth, width, comps);
    }}

    // Then scale vertically, to outHeight, into the out buffer.
    inOff = buffer;
    outOff = out;
    stride = outWidth * comps;
    inOffsetSize = comps;
    outOffsetSize = comps;
    { int i;
    for(i = 0; i < outWidth; ++i, inOff += inOffsetSize, outOff += outOffsetSize)
    {
        scaleLine(inOff, stride, outOff, stride, outHeight, height, comps);
    }}
    return out;
    }
}

uint8_t* GL_ScaleBufferNearest(const uint8_t* in, int width, int height, int comps,
    int outWidth, int outHeight)
{
    assert(in);
    {
    int ratioX, ratioY, shearY;
    uint8_t* out, *outP;

    if(width <= 0 || height <= 0)
        return (uint8_t*)in;

    ratioX = (int)(width  << 16) / outWidth  + 1;
    ratioY = (int)(height << 16) / outHeight + 1;

    if(NULL == (out = malloc(comps * outWidth * outHeight)))
        Con_Error("GL_ScaleBufferNearest: Failed on allocation of %lu bytes for "
                  "output buffer.", (unsigned long) (comps * outWidth * outHeight));

    outP = out;
    shearY = 0;
    { int i;
    for(i = 0; i < outHeight; ++i, shearY += ratioY)
    {
        int shearX = 0;
        int shearY2 = (shearY >> 16) * width;
        { int j;
        for(j = 0; j < outWidth; ++j, outP += comps, shearX += ratioX)
        {
            int c, n = (shearY2 + (shearX >> 16)) * comps;
            for(c = 0; c < comps; ++c, n++)
                outP[c] = in[n];
        }}
    }}
    return out;
    }
}

void GL_DownMipmap32(uint8_t* in, int width, int height, int comps)
{
    assert(in);
    {
    int x, y, c, outW = width >> 1, outH = height >> 1;
    uint8_t* out;

    if(width <= 0 || height <= 0 || comps <= 0)
        return;

    if(width == 1 && height == 1)
    {
#if _DEBUG
        Con_Error("GL_DownMipmap32: Can't be called for a 1x1 image.\n");
#endif
        return;
    }

    // Limited, 1x2|2x1 -> 1x1 reduction?
    if(!outW || !outH)
    {
        int outDim = (width > 1 ? outW : outH);

        out = in;
        for(x = 0; x < outDim; ++x, in += comps * 2)
            for(c = 0; c < comps; ++c, out++)
                *out = (uint8_t)((in[c] + in[comps + c]) >> 1);
        return;
    }

    // Unconstrained, 2x2 -> 1x1 reduction?
    out = in;
    for(y = 0; y < outH; ++y, in += width * comps)
        for(x = 0; x < outW; ++x, in += comps * 2)
            for(c = 0; c < comps; ++c, out++)
                *out = (uint8_t)((in[c] + in[comps + c] + in[comps * width + c] +
                              in[comps * (width + 1) + c]) >> 2);
    }
}

void GL_DownMipmap8(uint8_t* in, uint8_t* fadedOut, int width, int height, float fade)
{
    int x, y, outW = width / 2, outH = height / 2;
    float invFade;
    byte* out = in;

    if(fade > 1)
        fade = 1;
    invFade = 1 - fade;

    if(width == 1 && height == 1)
    {
#if _DEBUG
        Con_Error("GL_DownMipmap8: Can't be called for a 1x1 image.\n");
#endif
        return;
    }

    if(!outW || !outH)
    {   // Limited, 1x2|2x1 -> 1x1 reduction?
        int outDim = (width > 1 ? outW : outH);

        for(x = 0; x < outDim; x++, in += 2)
        {
            *out = (in[0] + in[1]) / 2;
            *fadedOut++ = (byte) (*out * invFade + 0x80 * fade);
            out++;
        }
    }
    else
    {   // Unconstrained, 2x2 -> 1x1 reduction?
        for(y = 0; y < outH; y++, in += width)
            for(x = 0; x < outW; x++, in += 2)
            {
                *out = (in[0] + in[1] + in[width] + in[width + 1]) / 4;
                *fadedOut++ = (byte) (*out * invFade + 0x80 * fade);
                out++;
            }
    }
}

void FindAverageLineColorIdx(const uint8_t* data, int w, int h, int line,
    colorpaletteid_t palid, boolean hasAlpha, float col[3])
{
    assert(data && col);
    {
    long count, numpels, avg[3] = { 0, 0, 0 };
    const uint8_t* start, *alphaStart;
    DGLubyte rgbUBV[3];
    DGLuint pal;

    if(w <= 0 || h <= 0)
    {
        col[CR] = col[CG] = col[CB] = 0;
        return;
    }

    if(line >= h)
    {
#if _DEBUG
        Con_Error("FindAverageLineColorIdx: Attempted to average outside valid area "
                  "(height=%i line=%i).", h, line);
#endif
        col[CR] = col[CG] = col[CB] = 0;
        return;
    }

    numpels = w * h;
    start = data + w * line;
    alphaStart = data + numpels + w * line;
    pal = R_GetColorPalette(palid);
    count = 0;
    { long i;
    for(i = 0; i < w; ++i)
    {
        if(!hasAlpha || alphaStart[i])
        {
            GL_GetColorPaletteRGB(pal, rgbUBV, start[i]);
            avg[CR] += rgbUBV[CR];
            avg[CG] += rgbUBV[CG];
            avg[CB] += rgbUBV[CB];
            ++count;
        }
    }}
    // All transparent? Sorry...
    if(!count)
        return;

    col[CR] = avg[CR] / count * reciprocal255;
    col[CG] = avg[CG] / count * reciprocal255;
    col[CB] = avg[CB] / count * reciprocal255;
    }
}

void FindAverageLineColor(const uint8_t* pixels, int width, int height,
    int pixelSize, int line, float col[3])
{
    assert(pixels && col);
    {
    long avg[3] = { 0, 0, 0 };
    const uint8_t* src;

    if(width <= 0 || height <= 0)
    {
        col[CR] = col[CG] = col[CB] = 0;
        return;
    }

    if(line >= height)
    {
#if _DEBUG
        Con_Error("FindAverageLineColor: Attempted to average outside valid area "
                  "(height=%i line=%i).", height, line);
#endif
        col[CR] = col[CG] = col[CB] = 0;
        return;
    }

    src = pixels + pixelSize * width * line;
    { int i;
    for(i = 0; i < width; ++i, src += pixelSize)
    {
        avg[CR] += src[CR];
        avg[CG] += src[CG];
        avg[CB] += src[CB];
    }}

    col[CR] = avg[CR] / width * reciprocal255;
    col[CG] = avg[CG] / width * reciprocal255;
    col[CB] = avg[CB] / width * reciprocal255;
    }
}

void FindAverageColor(const uint8_t* pixels, int width, int height,
    int pixelSize, float col[3])
{
    assert(pixels && col);
    {
    long numpels, avg[3] = { 0, 0, 0 };
    const uint8_t* src;

    if(width <= 0 || height <= 0)
    {
        col[CR] = col[CG] = col[CB] = 0;
        return;
    }

    if(pixelSize != 3 && pixelSize != 4)
    {
#if _DEBUG
        Con_Error("FindAverageColor: Attempted on non-rgb(a) image "
                  "(pixelSize=%i).", pixelSize);
#endif
        col[CR] = col[CG] = col[CB] = 0;
        return;
    }

    numpels = width * height;
    src = pixels;
    { long i;
    for(i = 0; i < numpels; ++i, src += pixelSize)
    {
        avg[CR] += src[CR];
        avg[CG] += src[CG];
        avg[CB] += src[CB];
    }}

    col[CR] = avg[CR] / numpels * reciprocal255;
    col[CG] = avg[CG] / numpels * reciprocal255;
    col[CB] = avg[CB] / numpels * reciprocal255;
    }
}

void FindAverageColorIdx(const uint8_t* data, int w, int h,
    colorpaletteid_t palid, boolean hasAlpha, float col[3])
{
    assert(data && col);
    {
    long numpels, count, avg[3] = { 0, 0, 0 };
    const uint8_t* alphaStart;
    DGLubyte rgbUBV[3];
    DGLuint pal;

    if(w <= 0 || h <= 0)
    {
        col[CR] = col[CG] = col[CB] = 0;
        return;
    }

    numpels = w * h;
    alphaStart = data + numpels;
    pal = R_GetColorPalette(palid);
    count = 0;
    { long i;
    for(i = 0; i < numpels; ++i)
    {
        if(!hasAlpha || alphaStart[i])
        {
            GL_GetColorPaletteRGB(pal, rgbUBV, data[i]);
            avg[CR] += rgbUBV[CR];
            avg[CG] += rgbUBV[CG];
            avg[CB] += rgbUBV[CB];
            ++count;
        }
    }}
    // All transparent? Sorry...
    if(0 == count)
        return;

    col[CR] = avg[CR] / count * reciprocal255;
    col[CG] = avg[CG] / count * reciprocal255;
    col[CB] = avg[CB] / count * reciprocal255;
    }
}

void FindClipRegionNonAlpha(const uint8_t* buffer, int width, int height,
    int pixelsize, int retRegion[4])
{
    assert(buffer && retRegion);
    {
    const uint8_t* src, *alphasrc;
    int region[4];

    if(width <= 0 || height <= 0)
    {
#if _DEBUG
        Con_Error("FindClipRegionNonAlpha: Attempt to find region on zero-sized image.");
#endif
        retRegion[0] = retRegion[1] = retRegion[2] = retRegion[3] = 0;
        return;
    }

    region[0] = width;
    region[1] = 0;
    region[2] = height;
    region[3] = 0;

    src = buffer;
    // For paletted images the alpha channel follows the actual image.
    if(pixelsize == 1)
        alphasrc = buffer + width * height;
    else
        alphasrc = NULL;

    // \todo This is not very efficent. Better to use an algorithm which works
    // on full rows and full columns.
    { int k, i;
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

            if(i < region[0])
                region[0] = i;
            if(i > region[1])
                region[1] = i;

            if(k < region[2])
                region[2] = k;
            if(k > region[3])
                region[3] = k;
        }
    }

    memcpy(retRegion, region, sizeof(retRegion));
    }
}

#if 0
void BlackOutlines(uint8_t* pixels, int width, int height)
{
    assert(pixels);
    {
    uint16_t* dark;
    int x, y, a, b;
    uint8_t* pix;
    long numpels;

    if(width <= 0 || height <= 0)
        return;

    numpels = width * height;
    dark = calloc(1, 2 * numpels);

    for(y = 1; y < height - 1; ++y)
    {
        for(x = 1; x < width - 1; ++x)
        {
            pix = pixels + (x + y * width) * 4;
            if(pix[3] > 128) // Not transparent.
            {
                // Apply darkness to surrounding transparent pixels.
                for(a = -1; a <= 1; ++a)
                {
                    for(b = -1; b <= 1; ++b)
                    {
                        uint8_t* other = pix + (a + b * width) * 4;
                        if(other[3] < 128) // Transparent.
                        {
                            dark[(x + a) + (y + b) * width] += 40;
                        }
                    }
                }
            }
        }
    }

    // Apply the darkness.
    { long i;
    for(i = 0, pix = pixels; i < numpels; ++i, pix += 4)
    {
        pix[3] = MIN_OF(255, (int)(pix[3] + dark[a]));
    }}

    // Release temporary storage.
    free(dark);
    }
}
#endif

/// \todo Not a very efficient algorithm...
void ColorOutlinesIdx(uint8_t* buffer, int width, int height)
{
    assert(buffer);
    {
    const int numpels = width * height;
    uint8_t* w[5];
    int x, y;

    if(width <= 0 || height <= 0)
        return;

    //      +----+
    //      | w0 |
    // +----+----+----+
    // | w1 | w2 | w3 |
    // +----+----+----+
    //      | w4 |
    //      +----+

    for(y = 0; y < height; ++y)
        for(x = 0; x < width; ++x)
        {
            // Only solid pixels spread.
            if(!buffer[numpels + x + y * width])
                continue;

            w[2] = buffer + x + y * width;

            w[0] = buffer + x + (y +        (y == 0? 0 : -1)) * width;
            w[4] = buffer + x + (y + (y == height-1? 0 :  1)) * width;

            w[1] = buffer + x +       (x == 0? 0 : -1) + (y)  * width;
            w[3] = buffer + x + (x == width-1? 0 :  1) + (y)  * width;

            if(w[0] != w[2] && !(*(w[0]+numpels)))
                *(w[0]) = *(w[2]);

            if(w[4] != w[2] && !(*(w[4]+numpels)))
                *(w[4]) = *(w[2]);

            if(w[1] != w[2] && !(*(w[1]+numpels)))
                *(w[1]) = *(w[2]);
 
            if(w[3] != w[2] && !(*(w[3]+numpels)))
                *(w[3]) = *(w[2]);
        }
    }
}

void EqualizeLuma(uint8_t* pixels, int width, int height, float* rBaMul,
    float* rHiMul, float* rLoMul)
{
    assert(pixels);
    {
    float hiMul, loMul, baMul;
    long wideAvg, numpels;
    uint8_t min, max, avg;
    uint8_t* pix;

    if(width <= 0 || height <= 0)
        return;

    numpels = width * height;
    min = 255;
    max = 0;
    wideAvg = 0;

    { long i;
    for(i = 0, pix = pixels; i < numpels; ++i, pix += 1)
    {
        if(*pix < min) min = *pix;
        if(*pix > max) max = *pix;
        wideAvg += *pix;
    }}

    if(max <= min || max == 0 || min == 255)
    {
        if(rBaMul) *rBaMul = -1;
        if(rHiMul) *rHiMul = -1;
        if(rLoMul) *rLoMul = -1;
        return; // Nothing we can do.
    }

    avg = MIN_OF(255, wideAvg / numpels);

    // Allow a small margin of variance with the balance multiplier.
    baMul = (!INRANGE_OF(avg, 127, 4)? (float)127/avg : 1);
    if(baMul != 1)
    {
        if(max < 255)
            max = (uint8_t) MINMAX_OF(1, (float)max - (255-max) * baMul, 255);
        if(min > 0)
            min = (uint8_t) MINMAX_OF(0, (float)min + min * baMul, 255);
    }

    hiMul = (max < 255?    (float)255/max  : 1);
    loMul = (min > 0  ? 1-((float)min/255) : 1);

    if(!(baMul == 1 && hiMul == 1 && loMul == 1))
    {
        long i;
        for(i = 0, pix = pixels; i < numpels; ++i, pix += 1)
        {
            // First balance.
            *pix = (uint8_t) MINMAX_OF(0, ((float)*pix) * baMul, 255);

            // Now amplify.
            if(*pix > 127)
                *pix = (uint8_t) MINMAX_OF(0, ((float)*pix) * hiMul, 255);
            else
                *pix = (uint8_t) MINMAX_OF(0, ((float)*pix) * loMul, 255);
        }
    }

    if(rBaMul) *rBaMul = baMul;
    if(rHiMul) *rHiMul = hiMul;
    if(rLoMul) *rLoMul = loMul;
    }
}

void Desaturate(uint8_t* pixels, int width, int height, int comps)
{
    assert(pixels);
    {
    uint8_t* pix;
    long i, numpels;

    if(width <= 0 || height <= 0)
        return;

    numpels = width * height;
    for(i = 0, pix = pixels; i < numpels; ++i, pix += comps)
    {
        int min = MIN_OF(pix[CR], MIN_OF(pix[CG], pix[CB]));
        int max = MAX_OF(pix[CR], MAX_OF(pix[CG], pix[CB]));
        pix[CR] = pix[CG] = pix[CB] = (min + max) / 2;
    }
    }
}

void AmplifyLuma(uint8_t* pixels, int width, int height, boolean hasAlpha)
{
    assert(pixels);
    {
    long numPels;
    uint8_t max = 0;

    if(width <= 0 || height <= 0)
        return;

    numPels = width * height;
    if(hasAlpha)
    {
        uint8_t* pix = pixels;
        uint8_t* apix = pixels + numPels;
        long i;
        for(i = 0; i < numPels; ++i, pix++, apix++)
        {
            // Only non-masked pixels count.
            if(!(*apix > 0))
                continue;

            if(*pix > max)
                max = *pix;
        }
    }
    else
    {
        uint8_t* pix = pixels;
        long i;
        for(i = 0; i < numPels; ++i, pix++)
        {
            if(*pix > max)
                max = *pix;
        }
    }

    if(0 == max || 255 == max)
        return;

    { uint8_t* pix = pixels;
    long i;
    for(i = 0; i < numPels; ++i, pix++)
    {
        *pix = (uint8_t) MINMAX_OF(0, (float)*pix / max * 255, 255);
    }}
    }
}

void EnhanceContrast(uint8_t* pixels, int width, int height, int comps)
{
    assert(pixels);
    {
    uint8_t* pix;
    long i, numpels;

    if(width <= 0 || height <= 0)
        return;

    if(comps != 3 && comps != 4)
    {
#if _DEBUG
        Con_Error("EnhanceContrast: Attempted on non-rgb(a) image (comps=%i).", comps);
#endif
        return;
    }

    pix = pixels;
    numpels = width * height;

    for(i = 0; i < numpels; ++i, pix += comps)
    {
        int c;
        for(c = 0; c < 3; ++c)
        {
            uint8_t out;
            if(pix[c] < 60) // Darken dark parts.
                out = (uint8_t) MINMAX_OF(0, ((float)pix[c] - 70) * 1.0125f + 70, 255);
            else if(pix[c] > 185) // Lighten light parts.
                out = (uint8_t) MINMAX_OF(0, ((float)pix[c] - 185) * 1.0125f + 185, 255);
            else
                out = pix[c];
            pix[c] = out;
        }
    }
    }
}

void SharpenPixels(uint8_t* pixels, int width, int height, int comps)
{
    assert(pixels);
    {
    const float strength = .05f;
    uint8_t* result;
    float A, B, C;
    int x, y;

    if(width <= 0 || height <= 0)
        return;

    if(comps != 3 && comps != 4)
    {
#if _DEBUG
        Con_Error("EnhanceContrast: Attempted on non-rgb(a) image (comps=%i).", comps);
#endif
        return;
    }

    result = calloc(1, comps * width * height);

    A = strength;
    B = .70710678 * strength; // 1/sqrt(2)
    C = 1 + 4*A + 4*B;

    for(y = 1; y < height - 1; ++y)
        for(x = 1; x < width -1; ++x)
        {
            const uint8_t* pix = pixels + (x + y*width) * comps;
            uint8_t* out = result + (x + y*width) * comps;
            int c;
            for(c = 0; c < 3; ++c)
            {
                int r = (C*pix[c] - A*pix[c - width] - A*pix[c + comps] - A*pix[c - comps] -
                         A*pix[c + width] - B*pix[c + comps - width] - B*pix[c + comps + width] -
                         B*pix[c - comps - width] - B*pix[c - comps + width]);
                out[c] = MINMAX_OF(0, r, 255);
            }

            if(comps == 4)
                out[3] = pix[3];
        }

    memcpy(pixels, result, comps * width * height);
    free(result);
    }
}

/**
 * @return  @c true, if the given color is either (0,255,255) or (255,0,255).
 */
static __inline boolean ColorKey(uint8_t* color)
{
    assert(color);
    return color[CB] == 0xff && ((color[CR] == 0xff && color[CG] == 0) ||
                                 (color[CR] == 0 && color[CG] == 0xff));
}

/**
 * Buffer must be RGBA. Doesn't touch the non-keyed pixels.
 */
static void DoColorKeying(uint8_t* rgbaBuf, int width)
{
    assert(rgbaBuf);
    { int i;
    for(i = 0; i < width; ++i, rgbaBuf += 4)
    {
        if(!ColorKey(rgbaBuf))
            continue;
        rgbaBuf[3] = rgbaBuf[2] = rgbaBuf[1] = rgbaBuf[0] = 0;
    }}
}

uint8_t* ApplyColorKeying(uint8_t* buf, int width, int height, int pixelSize)
{
    assert(buf);

    if(width <= 0 || height <= 0)
        return buf;

    // We must allocate a new buffer if the loaded image has less than the
    // required number of color components.
    if(pixelSize < 4)
    {
        const long numpels = width * height;
        uint8_t* ckdest = malloc(4 * numpels);
        uint8_t* in, *out;
        long i;
        for(in = buf, out = ckdest, i = 0; i < numpels; ++i, in += pixelSize, out += 4)
        {
            if(ColorKey(in))
            {
                memset(out, 0, 4); // Totally black.
                continue;
            }

            memcpy(out, in, 3); // The color itself.
            out[CA] = 255; // Opaque.
        }
        return ckdest;
    }

    // We can do the keying in-buffer.
    // This preserves the alpha values of non-keyed pixels.
    { int i;
    for(i = 0; i < height; ++i)
        DoColorKeying(buf + 4 * i * width, height);
    }
    return buf;
}
