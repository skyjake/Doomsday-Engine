/**\file gl_tex.cpp
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de_platform.h"
#include "gl/gl_tex.h"
#include "dd_main.h"

#include "misc/color.h"
#include "render/r_main.h"
#include "resource/clientresources.h"
#include "gl/sys_opengl.h"

#include <doomsday/resource/colorpalette.h>
#include <de/memory.h>
#include <de/memoryzone.h>
#include <de/vector1.h>
#include <de/texgamma.h>
#include <cstdlib>
#include <cmath>
#include <cctype>

static uint8_t *scratchBuffer;
static size_t scratchBufferSize;

/**
 * Provides a persistent scratch buffer for use by texture manipulation
 * routines e.g. scaleLine().
 */
static uint8_t *GetScratchBuffer(size_t size)
{
    // Need to enlarge?
    if(size > scratchBufferSize)
    {
        scratchBuffer = (uint8_t *) Z_Realloc(scratchBuffer, size, PU_APPSTATIC);
        scratchBufferSize = size;
    }
    return scratchBuffer;
}

/**
 * Len is measured in out units. Comps is the number of components per
 * pixel, or rather the number of bytes per pixel (3 or 4). The strides must
 * be byte-aligned anyway, though; not in pixels.
 *
 * @todo Probably could be optimized.
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

            for(c = 0; c < comps; ++c)
                out[c] = (uint8_t)((col1[c] * invWeight + col2[c] * weight) >> 16);
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
                    out[c] = (count? uint8_t(cumul[c] / count) : 0);
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
    for(i = outLen; i > 0; i--, out += outStride, in += inStride)
    {
        for(c = 0; c < comps; ++c)
            out[c] = in[c];
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

    buffer = GetScratchBuffer(comps * outWidth * height);

    out = (uint8_t *) M_Malloc(comps * outWidth * outHeight);

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

static void* packImage(int components, const float* tempOut, GLint typeOut,
    int widthOut, int heightOut, int sizeOut, int bpp, int packRowLength,
    int packAlignment, int packSkipRows, int packSkipPixels)
{
    int rowStride, rowLen;
    void* dataOut;

    dataOut = M_Malloc(bpp * widthOut * heightOut);

    if(packRowLength > 0)
    {
        rowLen = packRowLength;
    }
    else
    {
        rowLen = widthOut;
    }
    if(sizeOut >= packAlignment)
    {
        rowStride = components * rowLen;
    }
    else
    {
        rowStride = packAlignment / sizeOut
            * CEILING(components * rowLen * sizeOut, packAlignment);
    }

    switch(typeOut)
    {
    case GL_UNSIGNED_BYTE: {
        int i, j, k = 0;
        for(i = 0; i < heightOut; ++i)
        {
            GLubyte* ubptr = (GLubyte*) dataOut
                + i * rowStride
                + packSkipRows * rowStride + packSkipPixels * components;
            for(j = 0; j < widthOut * components; ++j)
            {
                *ubptr++ = (GLubyte) tempOut[k++];
            }
        }
        break;
      }
    case GL_BYTE: {
        int i, j, k = 0;
        for(i = 0; i < heightOut; i++)
        {
            GLbyte* bptr = (GLbyte*) dataOut
                + i * rowStride
                + packSkipRows * rowStride + packSkipPixels * components;
            for(j = 0; j < widthOut * components; ++j)
            {
                *bptr++ = (GLbyte) tempOut[k++];
            }
        }
        break;
      }
    case GL_UNSIGNED_SHORT: {
        int i, j, k = 0;
        for(i = 0; i < heightOut; ++i)
        {
            GLushort* usptr = (GLushort*) dataOut
                + i * rowStride
                + packSkipRows * rowStride + packSkipPixels * components;
            for(j = 0; j < widthOut * components; ++j)
            {
                *usptr++ = (GLushort) tempOut[k++];
            }
        }
        break;
      }
    case GL_SHORT: {
        int i, j, k = 0;
        for(i = 0; i < heightOut; ++i)
        {
            GLshort* sptr = (GLshort*) dataOut
                + i * rowStride
                + packSkipRows * rowStride + packSkipPixels * components;
            for(j = 0; j < widthOut * components; ++j)
            {
                *sptr++ = (GLshort) tempOut[k++];
            }
        }
        break;
      }
    case GL_UNSIGNED_INT: {
        int i, j, k = 0;
        for(i = 0; i < heightOut; ++i)
        {
            GLuint* uiptr = (GLuint*) dataOut
                + i * rowStride
                + packSkipRows * rowStride + packSkipPixels * components;
            for(j = 0; j < widthOut * components; ++j)
            {
                *uiptr++ = (GLuint) tempOut[k++];
            }
        }
        break;
      }
    case GL_INT: {
        int i, j, k = 0;
        for(i = 0; i < heightOut; ++i)
        {
            GLint* iptr = (GLint*) dataOut
                + i * rowStride
                + packSkipRows * rowStride + packSkipPixels * components;
            for(j = 0; j < widthOut * components; ++j)
            {
                *iptr++ = (GLint) tempOut[k++];
            }
        }
        break;
      }
    case GL_FLOAT: {
        int i, j, k = 0;
        for(i = 0; i < heightOut; ++i)
        {
            GLfloat* fptr = (GLfloat*) dataOut
                + i * rowStride
                + packSkipRows * rowStride + packSkipPixels * components;
            for (j = 0; j < widthOut * components; ++j)
            {
                *fptr++ = tempOut[k++];
            }
        }
        break;
      }
    default:
        DENG_ASSERT(!"packImage: Unknown output type");
        return 0;
    }

    return dataOut;
}

/**
 * Originally from the Mesa 3-D graphics library version 3.4
 * @note License: GNU Library General Public License (or later)
 * Copyright (C) 1995-2000  Brian Paul.
 */
void* GL_ScaleBufferEx(const void* dataIn, int widthIn, int heightIn, int bpp,
    /*GLint typeIn,*/ int unpackRowLength, int unpackAlignment, int unpackSkipRows,
    int unpackSkipPixels, int widthOut, int heightOut, /*GLint typeOut, */
    int packRowLength, int packAlignment, int packSkipRows, int packSkipPixels)
{
    const GLint typeIn = GL_UNSIGNED_BYTE, typeOut = GL_UNSIGNED_BYTE;
    int i, j, k, sizeIn, sizeOut, rowStride, rowLen;
    float* tempIn, *tempOut;
    float sx, sy;
    void* dataOut;

    // Determine bytes per input datum.
    switch(typeIn)
    {
    case GL_UNSIGNED_BYTE:
        sizeIn = sizeof(GLubyte);
        break;
    case GL_BYTE:
        sizeIn = sizeof(GLbyte);
        break;
    case GL_UNSIGNED_SHORT:
        sizeIn = sizeof(GLushort);
        break;
    case GL_SHORT:
        sizeIn = sizeof(GLshort);
        break;
    case GL_UNSIGNED_INT:
        sizeIn = sizeof(GLuint);
        break;
    case GL_INT:
        sizeIn = sizeof(GLint);
        break;
    case GL_FLOAT:
        sizeIn = sizeof(GLfloat);
        break;
    default:
        return NULL;
    }

    // Determine bytes per output datum.
    switch(typeOut)
    {
    case GL_UNSIGNED_BYTE:
        sizeOut = sizeof(GLubyte);
        break;
    case GL_BYTE:
        sizeOut = sizeof(GLbyte);
        break;
    case GL_UNSIGNED_SHORT:
        sizeOut = sizeof(GLushort);
        break;
    case GL_SHORT:
        sizeOut = sizeof(GLshort);
        break;
    case GL_UNSIGNED_INT:
        sizeOut = sizeof(GLuint);
        break;
    case GL_INT:
        sizeOut = sizeof(GLint);
        break;
    case GL_FLOAT:
        sizeOut = sizeof(GLfloat);
        break;
    default:
        return NULL;
    }

    // Allocate storage for intermediate images.
    tempIn = (float *) M_Malloc(widthIn * heightIn * bpp * sizeof(float));

    tempOut = (float *) M_Malloc(widthOut * heightOut * bpp * sizeof(float));

    /**
     * Unpack the pixel data and convert to floating point
     */

    if(unpackRowLength > 0)
    {
        rowLen = unpackRowLength;
    }
    else
    {
        rowLen = widthIn;
    }

    if(sizeIn >= unpackAlignment)
    {
        rowStride = bpp * rowLen;
    }
    else
    {
        rowStride = unpackAlignment / sizeIn
            * CEILING(bpp * rowLen * sizeIn, unpackAlignment);
    }

    switch(typeIn)
    {
    case GL_UNSIGNED_BYTE:
        k = 0;
        for(i = 0; i < heightIn; ++i)
        {
            GLubyte* ubptr = (GLubyte*) dataIn
                + i * rowStride
                + unpackSkipRows * rowStride + unpackSkipPixels * bpp;
            for(j = 0; j < widthIn * bpp; ++j)
            {
                tempIn[k++] = (float) *ubptr++;
            }
        }
        break;
    case GL_BYTE:
        k = 0;
        for(i = 0; i < heightIn; ++i)
        {
            GLbyte* bptr = (GLbyte*) dataIn
                + i * rowStride
                + unpackSkipRows * rowStride + unpackSkipPixels * bpp;
            for(j = 0; j < widthIn * bpp; ++j)
            {
                tempIn[k++] = (float) *bptr++;
            }
        }
        break;
    case GL_UNSIGNED_SHORT:
        k = 0;
        for(i = 0; i < heightIn; ++i)
        {
            GLushort* usptr = (GLushort*) dataIn
                + i * rowStride
                + unpackSkipRows * rowStride + unpackSkipPixels * bpp;
            for(j = 0; j < widthIn * bpp; ++j)
            {
                tempIn[k++] = (float) *usptr++;
            }
        }
        break;
    case GL_SHORT:
        k = 0;
        for(i = 0; i < heightIn; ++i)
        {
            GLshort* sptr = (GLshort*) dataIn
                + i * rowStride
                + unpackSkipRows * rowStride + unpackSkipPixels * bpp;
            for(j = 0; j < widthIn * bpp; ++j)
            {
                tempIn[k++] = (float) *sptr++;
            }
        }
        break;
    case GL_UNSIGNED_INT:
        k = 0;
        for(i = 0; i < heightIn; ++i)
        {
            GLuint* uiptr = (GLuint*) dataIn
                + i * rowStride
                + unpackSkipRows * rowStride + unpackSkipPixels * bpp;
            for(j = 0; j < widthIn * bpp; ++j)
            {
                tempIn[k++] = (float) *uiptr++;
            }
        }
        break;
    case GL_INT:
        k = 0;
        for(i = 0; i < heightIn; ++i)
        {
            GLint* iptr = (GLint*) dataIn
                + i * rowStride
                + unpackSkipRows * rowStride + unpackSkipPixels * bpp;
            for(j = 0; j < widthIn * bpp; ++j)
            {
                tempIn[k++] = (float) *iptr++;
            }
        }
        break;
    case GL_FLOAT:
        k = 0;
        for(i = 0; i < heightIn; ++i)
        {
            GLfloat* fptr = (GLfloat*) dataIn
                + i * rowStride
                + unpackSkipRows * rowStride + unpackSkipPixels * bpp;
            for(j = 0; j < widthIn * bpp; ++j)
            {
                tempIn[k++] = *fptr++;
            }
        }
        break;
    default:
        return 0;
    }

    /**
     * Scale the image!
     */

    if(widthOut > 1)
        sx = (float) (widthIn - 1) / (float) (widthOut - 1);
    else
        sx = (float) (widthIn - 1);
    if(heightOut > 1)
        sy = (float) (heightIn - 1) / (float) (heightOut - 1);
    else
        sy = (float) (heightIn - 1);

    if(sx < 1.0 && sy < 1.0)
    {
        // Magnify both width and height: use weighted sample of 4 pixels.
        int i0, i1, j0, j1;
        float alpha, beta;
        float* src00, *src01, *src10, *src11;
        float s1, s2;
        float* dst;

        for(i = 0; i < heightOut; ++i)
        {
            i0 = i * sy;
            i1 = i0 + 1;
            if(i1 >= heightIn)
                i1 = heightIn - 1;
            alpha = i * sy - i0;
            for(j = 0; j < widthOut; ++j)
            {
                j0 = j * sx;
                j1 = j0 + 1;
                if(j1 >= widthIn)
                    j1 = widthIn - 1;
                beta = j * sx - j0;

                // Compute weighted average of pixels in rect (i0,j0)-(i1,j1)
                src00 = tempIn + (i0 * widthIn + j0) * bpp;
                src01 = tempIn + (i0 * widthIn + j1) * bpp;
                src10 = tempIn + (i1 * widthIn + j0) * bpp;
                src11 = tempIn + (i1 * widthIn + j1) * bpp;

                dst = tempOut + (i * widthOut + j) * bpp;

                for (k = 0; k < bpp; ++k)
                {
                    s1 = *src00++ * (1.0 - beta) + *src01++ * beta;
                    s2 = *src10++ * (1.0 - beta) + *src11++ * beta;
                    *dst++ = s1 * (1.0 - alpha) + s2 * alpha;
                }
            }
        }
    }
    else
    {
        // Shrink width and/or height:  use an unweighted box filter.
        int i0, i1;
        int j0, j1;
        int ii, jj;
        float sum, *dst;

        for(i = 0; i < heightOut; ++i)
        {
            i0 = i * sy;
            i1 = i0 + 1;
            if(i1 >= heightIn)
                i1 = heightIn - 1;

            for(j = 0; j < widthOut; ++j)
            {
                j0 = j * sx;
                j1 = j0 + 1;
                if(j1 >= widthIn)
                    j1 = widthIn - 1;

                dst = tempOut + (i * widthOut + j) * bpp;

                // Compute average of pixels in the rectangle (i0,j0)-(i1,j1)
                for(k = 0; k < bpp; ++k)
                {
                    sum = 0.0;
                    for(ii = i0; ii <= i1; ++ii)
                    {
                        for(jj = j0; jj <= j1; ++jj)
                        {
                            sum += *(tempIn + (ii * widthIn + jj) * bpp + k);
                        }
                    }
                    sum /= (j1 - j0 + 1) * (i1 - i0 + 1);
                    *dst++ = sum;
                }
            }
        }
    }

    // Free temporary image storage.
    free(tempIn);

    /**
     * Return output image.
     */
    dataOut = packImage(bpp, tempOut, typeOut, widthOut, heightOut, sizeOut, bpp,
        packRowLength, packAlignment, packSkipRows, packSkipPixels);

    // Free temporary image storage.
    free(tempOut);

    return dataOut;
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

    out = (uint8_t *) M_Malloc(comps * outWidth * outHeight);

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
        DENG_ASSERT(!"GL_DownMipmap32: Can't be called for a 1x1 image.");
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
        DENG_ASSERT(!"GL_DownMipmap8: Can't be called for a 1x1 image.");
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

dd_bool GL_PalettizeImage(uint8_t *out, int outformat, res::ColorPalette const *palette,
    dd_bool applyTexGamma, uint8_t const *in, int informat, int width, int height)
{
    DENG2_ASSERT(in && out && palette);

    if(width <= 0 || height <= 0)
        return false;

    if(informat <= 2 && outformat >= 3)
    {
        long const numPels = width * height;
        int const inSize   = (informat == 2 ? 1 : informat);
        int const outSize  = (outformat == 2 ? 1 : outformat);

        for(long i = 0; i < numPels; ++i)
        {
            de::Vector3ub palColor = palette->color(*in);

            out[0] = palColor.x;
            out[1] = palColor.y;
            out[2] = palColor.z;

            if(applyTexGamma)
            {
                out[0] = R_TexGammaLut(out[0]);
                out[1] = R_TexGammaLut(out[1]);
                out[2] = R_TexGammaLut(out[2]);
            }

            if(outformat == 4)
            {
                if(informat == 2)
                    out[3] = in[numPels * inSize];
                else
                    out[3] = 0;
            }

            in  += inSize;
            out += outSize;
        }
        return true;
    }
    return false;
}

dd_bool GL_QuantizeImageToPalette(uint8_t *out, int outformat, res::ColorPalette const *palette,
    uint8_t const *in, int informat, int width, int height)
{
    DENG2_ASSERT(out != 0 && in != 0 && palette != 0);

    if(informat >= 3 && outformat <= 2 && width > 0 && height > 0)
    {
        int inSize = (informat == 2 ? 1 : informat);
        int outSize = (outformat == 2 ? 1 : outformat);
        int i, numPixels = width * height;

        for(i = 0; i < numPixels; ++i, in += inSize, out += outSize)
        {
            // Convert the color value.
            *out = palette->nearestIndex(de::Vector3ub(in));

            // Alpha channel?
            if(outformat == 2)
            {
                if(informat == 4)
                    out[numPixels * outSize] = in[3];
                else
                    out[numPixels * outSize] = 0;
            }
        }
        return true;
    }
    return false;
}

void GL_DeSaturatePalettedImage(uint8_t *pixels, res::ColorPalette const &palette,
    int width, int height)
{
    DENG2_ASSERT(pixels != 0);

    if(!width || !height)  return;

    long const numPels = width * height;

    // What is the maximum color value?
    int max = 0;
    for(long i = 0; i < numPels; ++i)
    {
        de::Vector3ub palColor = palette[pixels[i]];
        if(palColor.x == palColor.y && palColor.x == palColor.z)
        {
            if(palColor.x > max)
            {
                max = palColor.x;
            }
            continue;
        }

        int temp = (2 * int( palColor.x ) + 4 * int( palColor.y ) + 3 * int( palColor.z )) / 9;
        if(temp > max) max = temp;
    }

    for(long i = 0; i < numPels; ++i)
    {
        de::Vector3ub palColor = palette[pixels[i]];
        if(palColor.x == palColor.y && palColor.x == palColor.z)
        {
            continue;
        }

        // Calculate a weighted average.
        int temp = (2 * int( palColor.x ) + 4 * int( palColor.y ) + 3 * int( palColor.z )) / 9;
        if(max) temp *= 255.f / max;

        pixels[i] = palette.nearestIndex(de::Vector3ub(temp, temp, temp));
    }
}

void FindAverageLineColorIdx(uint8_t const *data, int w, int h, int line,
    res::ColorPalette const &palette, dd_bool hasAlpha, ColorRawf *color)
{
    DENG2_ASSERT(data != 0 && color != 0);

    long i, count, numpels, avg[3] = { 0, 0, 0 };
    uint8_t const *start, *alphaStart;

    if(w <= 0 || h <= 0)
    {
        V3f_Set(color->rgb, 0, 0, 0);
        return;
    }

    if(line >= h)
    {
        App_Log(DE2_DEV_GL_ERROR, "FindAverageLineColorIdx: height=%i, line=%i.", h, line);
        DENG_ASSERT(!"FindAverageLineColorIdx: Attempted to average outside valid area.");
        V3f_Set(color->rgb, 0, 0, 0);
        return;
    }

    numpels = w * h;
    start = data + w * line;
    alphaStart = data + numpels + w * line;
    count = 0;
    for(i = 0; i < w; ++i)
    {
        if(!hasAlpha || alphaStart[i])
        {
            de::Vector3ub palColor = palette[start[i]];
            avg[0] += palColor.x;
            avg[1] += palColor.y;
            avg[2] += palColor.z;
            ++count;
        }
    }

    // All transparent? Sorry...
    if(!count) return;

    V3f_Set(color->rgb, avg[0] / count * reciprocal255,
                        avg[1] / count * reciprocal255,
                        avg[2] / count * reciprocal255);
}

void FindAverageLineColor(const uint8_t* pixels, int width, int height,
    int pixelSize, int line, ColorRawf* color)
{
    long avg[3] = { 0, 0, 0 };
    const uint8_t* src;
    int i;
    assert(pixels && color);

    if(width <= 0 || height <= 0)
    {
        V3f_Set(color->rgb, 0, 0, 0);
        return;
    }

    if(line >= height)
    {
        App_Log(DE2_DEV_GL_ERROR, "EnhanceContrast: height=%i, line=%i.", height, line);
        DENG_ASSERT(!"FindAverageLineColor: Attempted to average outside valid area.");

        V3f_Set(color->rgb, 0, 0, 0);
        return;
    }

    src = pixels + pixelSize * width * line;
    for(i = 0; i < width; ++i, src += pixelSize)
    {
        avg[0] += src[0];
        avg[1] += src[1];
        avg[2] += src[2];
    }

    V3f_Set(color->rgb, avg[0] / width * reciprocal255,
                        avg[1] / width * reciprocal255,
                        avg[2] / width * reciprocal255);
}

void FindAverageColor(const uint8_t* pixels, int width, int height,
    int pixelSize, ColorRawf* color)
{
    long i, numpels, avg[3] = { 0, 0, 0 };
    const uint8_t* src;
    assert(pixels && color);

    if(width <= 0 || height <= 0)
    {
        V3f_Set(color->rgb, 0, 0, 0);
        return;
    }

    if(pixelSize != 3 && pixelSize != 4)
    {
        App_Log(DE2_DEV_GL_ERROR, "FindAverageColor: pixelSize=%i", pixelSize);
        DENG_ASSERT("FindAverageColor: Attempted on non-rgb(a) image.");

        V3f_Set(color->rgb, 0, 0, 0);
        return;
    }

    numpels = width * height;
    src = pixels;
    for(i = 0; i < numpels; ++i, src += pixelSize)
    {
        avg[0] += src[0];
        avg[1] += src[1];
        avg[2] += src[2];
    }

    V3f_Set(color->rgb, avg[0] / numpels * reciprocal255,
                        avg[1] / numpels * reciprocal255,
                        avg[2] / numpels * reciprocal255);
}

void FindAverageColorIdx(uint8_t const *data, int w, int h, res::ColorPalette const &palette,
    dd_bool hasAlpha, ColorRawf *color)
{
    DENG2_ASSERT(data != 0 && color != 0);

    long i, numpels, count, avg[3] = { 0, 0, 0 };
    uint8_t const *alphaStart;

    if(w <= 0 || h <= 0)
    {
        V3f_Set(color->rgb, 0, 0, 0);
        return;
    }

    numpels = w * h;
    alphaStart = data + numpels;
    count = 0;
    for(i = 0; i < numpels; ++i)
    {
        if(!hasAlpha || alphaStart[i])
        {
            de::Vector3ub palColor = palette[data[i]];
            avg[0] += palColor.x;
            avg[1] += palColor.y;
            avg[2] += palColor.z;
            ++count;
        }
    }

    // All transparent? Sorry...
    if(0 == count) return;

    V3f_Set(color->rgb, avg[0] / count * reciprocal255,
                        avg[1] / count * reciprocal255,
                        avg[2] / count * reciprocal255);
}

void FindAverageAlpha(const uint8_t* pixels, int width, int height,
                      int pixelSize, float* alpha, float* coverage)
{
    long i, numPels, avg = 0, alphaCount = 0;
    const uint8_t* src;

    if(!pixels || !alpha) return;

    if(width <= 0 || height <= 0)
    {
        // Transparent.
        *alpha = 0;
        if(coverage) *coverage = 1;
        return;
    }

    if(pixelSize != 3 && pixelSize != 4)
    {
        App_Log(DE2_DEV_GL_ERROR, "FindAverageAlpha: pixelSize=%i", pixelSize);
        DENG_ASSERT(!"FindAverageAlpha: Attempted on non-rgb(a) image.");

        // Assume opaque.
        *alpha = 1;
        if(coverage) *coverage = 0;
        return;
    }

    if(pixelSize == 3)
    {
        // Opaque. Well that was easy...
        *alpha = 1;
        if(coverage) *coverage = 0;
        return;
    }

    numPels = width * height;
    src = pixels;
    for(i = 0; i < numPels; ++i, src += 4)
    {
        const uint8_t val = src[3];
        avg += val;
        if(val < 255) alphaCount++;
    }

    *alpha = avg / numPels * reciprocal255;

    // Calculate coverage?
    if(coverage) *coverage = (float)alphaCount / numPels;
}

void FindAverageAlphaIdx(uint8_t const *pixels, int w, int h, float *alpha,
    float *coverage)
{
    long i, numPels, avg = 0, alphaCount = 0;
    uint8_t const *alphaStart;

    if(!pixels || !alpha) return;

    if(w <= 0 || h <= 0)
    {
        // Transparent.
        *alpha = 0;
        if(coverage) *coverage = 1;
        return;
    }

    numPels = w * h;
    alphaStart = pixels + numPels;
    for(i = 0; i < numPels; ++i)
    {
        uint8_t const val = alphaStart[i];
        avg += val;
        if(val < 255)
        {
            alphaCount++;
        }
    }

    *alpha = avg / numPels * reciprocal255;

    // Calculate coverage?
    if(coverage) *coverage = (float)alphaCount / numPels;
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
        DENG_ASSERT(!"FindClipRegionNonAlpha: Attempt to find region on zero-sized image.");

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

    retRegion[0] = region[0];
    retRegion[1] = region[1];
    retRegion[2] = region[2];
    retRegion[3] = region[3];
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

void ColorOutlinesIdx(uint8_t* buffer, int width, int height)
{
    DENG_ASSERT(buffer);

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

    /// @todo Not a very efficient algorithm...

    for(y = 0; y < height; ++y)
    {
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

void ColorOutlinesRGBA(uint8_t *buffer, int width, int height)
{
    using namespace de;

    uint32_t *buffer32 = reinterpret_cast<uint32_t *>(buffer);

    for (int y = 0; y < height; ++y)
    {
        uint32_t *row = buffer32 + y * width;
        for (int x = 0; x < width; ++x)
        {
            if ((row[x] & 0xff000000) == 0) // transparent pixel
            {
                int average[3]{};
                int count = 0;
                for (int dy = -1; dy <= 1; ++dy)
                {
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        if (!(dx || dy)) continue; // the current pixel

                        const Vec2i pos(x + dx, y + dy);
                        if (pos.x >= 0 && pos.y >= 0 && pos.x < width && pos.y < height)
                        {
                            const uint32_t adjacent = buffer32[pos.y * width + pos.x];
                            if (adjacent & 0xff000000) // non-transparent pixel
                            {
                                average[0] += adjacent & 0xff;
                                average[1] += (adjacent >> 8) & 0xff;
                                average[2] += (adjacent >> 16) & 0xff;
                                ++count;
                            }
                        }
                    }
                }
                if (count)
                {
                    for (int &c : average) c /= count;
                }
                row[x] = average[0] | (average[1] << 8) | (average[2] << 16);
            }
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
            float val = baMul * (*pix);
            // Now amplify.
            if(val > 127) val *= hiMul;
            else          val *= loMul;

            *pix = (uint8_t) MINMAX_OF(0, val, 255);
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
        int min = MIN_OF(pix[0], MIN_OF(pix[1], pix[2]));
        int max = MAX_OF(pix[0], MAX_OF(pix[1], pix[2]));
        pix[0] = pix[1] = pix[2] = (min + max) / 2;
    }
    }
}

void AmplifyLuma(uint8_t* pixels, int width, int height, dd_bool hasAlpha)
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
        App_Log(DE2_DEV_GL_ERROR, "EnhanceContrast: comps=%i", comps);
        DENG_ASSERT(!"EnhanceContrast: Attempted on non-rgb(a) image.");
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
        App_Log(DE2_DEV_GL_ERROR, "SharpenPixels: comps=%i", comps);
        DENG_ASSERT(!"SharpenPixels: Attempted on non-rgb(a) image.");
        return;
    }

    result = (uint8_t *) M_Calloc(comps * width * height);

    A = strength;
    B = .70710678f * strength; // 1/sqrt(2)
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
static inline bool isKeyedColor(uint8_t *color)
{
    DENG2_ASSERT(color);
    return color[2] == 0xff && ((color[0] == 0xff && color[1] == 0) ||
                                 (color[0] == 0 && color[1] == 0xff));
}

/**
 * Buffer must be RGBA. Doesn't touch the non-keyed pixels.
 */
static void doColorKeying(uint8_t *rgbaBuf, int width)
{
    DENG2_ASSERT(rgbaBuf);

    for(int i = 0; i < width; ++i, rgbaBuf += 4)
    {
        if(!isKeyedColor(rgbaBuf)) continue;

        rgbaBuf[3] = rgbaBuf[2] = rgbaBuf[1] = rgbaBuf[0] = 0;
    }
}

uint8_t *ApplyColorKeying(uint8_t *buf, int width, int height, int pixelSize)
{
    DENG2_ASSERT(buf);

    if(width <= 0 || height <= 0)
        return buf;

    // We must allocate a new buffer if the loaded image has less than the
    // required number of color components.
    if(pixelSize < 4)
    {
        long const numpels = width * height;
        uint8_t *ckdest = (uint8_t *) M_Malloc(4 * numpels);
        uint8_t *in, *out;
        long i;

        for(in = buf, out = ckdest, i = 0; i < numpels; ++i, in += pixelSize, out += 4)
        {
            if(isKeyedColor(in))
            {
                std::memset(out, 0, 4); // Totally black.
                continue;
            }

            std::memcpy(out, in, 3); // The color itself.
            out[3] = 255; // Opaque.
        }
        return ckdest;
    }

    // We can do the keying in-buffer.
    // This preserves the alpha values of non-keyed pixels.
    for(int i = 0; i < height; ++i)
    {
        doColorKeying(buf + 4 * i * width, height);
    }
    return buf;
}
