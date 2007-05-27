/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
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

/*
 * gl_tex.c: Image manipulation algorithms.
 *
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

#define RGB18(r, g, b) ((r)+((g)<<6)+((b)<<12))

// TYPES -------------------------------------------------------------------

// posts are runs of non masked source pixels
typedef struct {
    byte            topdelta;  // -1 is the last post in a column
    byte            length;
    // length data bytes follows
} post_t;

// column_t is a list of 0 or more post_t, (byte)-1 terminated
typedef post_t  column_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void    averageColorIdx(rgbcol_t * sprcol, byte *data, int w, int h,
                        byte *palette, boolean has_alpha);
void    averageColorRGB(rgbcol_t * col, byte *data, int w, int h);

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
        scratchBuffer = Z_Realloc(scratchBuffer, size, PU_STATIC);
        scratchBufferSize = size;
    }

    return scratchBuffer;
}

/**
 * Finds the power of 2 that is equal to or greater than the specified
 * number.
 */
int CeilPow2(int num)
{
    int         cumul;

    for(cumul = 1; num > cumul; cumul <<= 1);

    return cumul;
}

/**
 * Finds the power of 2 that is less than or equal to the specified number.
 */
int FloorPow2(int num)
{
    int         fl = CeilPow2(num);

    if(fl > num)
        fl >>= 1;
    return fl;
}

/**
 * Finds the power of 2 that is nearest the specified number. In ambiguous
 * cases, the smaller number is returned.
 */
int RoundPow2(int num)
{
    int         cp2 = CeilPow2(num), fp2 = FloorPow2(num);

    return ((cp2 - num >= num - fp2) ? fp2 : cp2);
}

/**
 * Weighted rounding. Weight determines the point where the number is still
 * rounded down (0..1).
 */
int WeightPow2(int num, float weight)
{
    int         fp2 = FloorPow2(num);
    float       frac = (num - fp2) / (float) fp2;

    if(frac <= weight)
        return fp2;
    else
        return (fp2 << 1);
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
 * Prepare the pal18to8 table.
 * A time-consuming operation (64 * 64 * 64 * 256!).
 */
void CalculatePal18to8(byte *dest, byte *palette)
{
    int         r, g, b, i;
    byte        palRGB[3];
    unsigned int diff, smallestDiff, closestIndex = 0;

    for(r = 0; r < 64; ++r)
        for(g = 0; g < 64; ++g)
            for(b = 0; b < 64; ++b)
            {
                // We must find the color index that most closely
                // resembles this RGB combination.
                smallestDiff = -1;
                for(i = 0; i < 256; ++i)
                {
                    memcpy(palRGB, palette + 3 * i, 3);
                    diff =
                        (palRGB[0] - (r << 2)) * (palRGB[0] - (r << 2)) +
                        (palRGB[1] - (g << 2)) * (palRGB[1] - (g << 2)) +
                        (palRGB[2] - (b << 2)) * (palRGB[2] - (b << 2));
                    if(diff < smallestDiff)
                    {
                        smallestDiff = diff;
                        closestIndex = i;
                    }
                }
                dest[RGB18(r, g, b)] = closestIndex;
            }
}

void PalIdxToRGB(byte *pal, int idx, byte *rgb)
{
    int         c;
    int         gammalevel = /*gammaSupport? 0 : */ usegamma;

    for(c = 0; c < 3; ++c)      // Red, green and blue.
        rgb[c] = gammatable[gammalevel][pal[idx * 3 + c]];
}

/**
 * in/out format:
 * 1 = palette indices
 * 2 = palette indices followed by alpha values
 * 3 = RGB
 * 4 = RGBA
 */
void GL_ConvertBuffer(int width, int height, int informat, int outformat,
                      byte *in, byte *out, byte *palette, boolean gamma)
{
    int         inSize = (informat == 2 ? 1 : informat);
    int         outSize = (outformat == 2 ? 1 : outformat);
    int         i, numPixels = width * height, a;

    if(informat == outformat)
    {
        // No conversion necessary.
        memcpy(out, in, numPixels * informat);
        return;
    }

    // Conversion from pal8(a) to RGB(A).
    if(informat <= 2 && outformat >= 3)
    {
        for(i = 0; i < numPixels; ++i, in += inSize, out += outSize)
        {
            // Copy the RGB values in every case.
            if(gamma)
            {
                for(a = 0; a < 3; ++a)
                    out[a] = gammatable[usegamma][*(palette + 3 * (*in) + a)];
            }
            else
            {
                memcpy(out, palette + 3 * (*in), 3);
            }
            // Will the alpha channel be necessary?
            a = 0;
            if(informat == 2)
                a = in[numPixels * inSize];
            if(outformat == 4)
                out[3] = (byte) a;
        }
    }
    // Conversion from RGB(A) to pal8(a), using pal18to8.
    else if(informat >= 3 && outformat <= 2)
    {
        byte *pal18to8 = GL_GetPal18to8();

        for(i = 0; i < numPixels; ++i, in += inSize, out += outSize)
        {
            // Convert the color value.
            *out = pal18to8[RGB18(in[0] >> 2, in[1] >> 2, in[2] >> 2)];
            // Alpha channel?
            a = 0;
            if(informat == 4)
                a = in[3];
            if(outformat == 2)
                out[numPixels * outSize] = (byte) a;
        }
    }
    else if(informat == 3 && outformat == 4)
    {
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
 * FIXME: Probably could be optimized.
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
 * @param true          If <code>noStretch == true</code> and the stretching
 *                      can be skipped.
 */
boolean GL_OptimalSize(int width, int height, int *optWidth, int *optHeight,
                       boolean noStretch)
{
    if(noStretch)
    {
        *optWidth = CeilPow2(width);
        *optHeight = CeilPow2(height);

        // MaxTexSize may prevent using noStretch.
        if(*optWidth > glMaxTexSize)
        {
            *optWidth = glMaxTexSize;
            noStretch = false;
        }
        if(*optHeight > glMaxTexSize)
        {
            *optHeight = glMaxTexSize;
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
            *optWidth = CeilPow2(width);
            *optHeight = CeilPow2(height);
        }
        else if(texQuality == 0)
        {
            // At the lowest quality, all textures are sized down
            // to the nearest power of 2.
            *optWidth = FloorPow2(width);
            *optHeight = FloorPow2(height);
        }
        else
        {
            // At the other quality *opts, a weighted rounding
            // is used.
            *optWidth = WeightPow2(width, 1 - texQuality / (float) TEXQ_BEST);
            *optHeight =
                WeightPow2(height, 1 - texQuality / (float) TEXQ_BEST);
        }
    }

    // Hardware limitations may force us to modify the preferred
    // texture size.
    if(*optWidth > glMaxTexSize)
        *optWidth = glMaxTexSize;
    if(*optHeight > glMaxTexSize)
        *optHeight = glMaxTexSize;
    if(ratioLimit)
    {
        if(*optWidth > *optHeight)  // Wide texture.
        {
            if(*optHeight < *optWidth / ratioLimit)
                *optHeight = *optWidth / ratioLimit;
        }
        else                    // Tall texture.
        {
            if(*optWidth < *optHeight / ratioLimit)
                *optWidth = *optHeight / ratioLimit;
        }
    }

    return noStretch;
}

/**
 * Modified to allow taller masked textures - GMJ Aug 2002
 *
 * Warning: The buffer must have room for the new alpha data!
 *
 * @param buffer        The destination buffer to draw the patch in to.
 * @param texwidth      Width of the dst buffer in pixels.
 * @param texheight     Height of the dst buffer in pixels.
 * @param patch         Ptr to the patch structure to draw to the dst buffer.
 * @param origx         X coordinate in the dst buffer to draw the patch too.
 * @param origy         Y coordinate in the dst buffer to draw the patch too.
 * @param maskZero      Used with sky textures.
 * @param transtable    If not <code>NULL</code>, read pixels from the patch
 *                      will have their color translated using this LUT.
 * @param checkForAlpha If <code>true</code> the composited image will be
 *                      checked for alpha pixels and will return accordingly
 *                      if present.
 *
 * @return              If <code>checkForAlpha == false</code>, will return
 *                      <code>false</code>. Else, <code>true</code> if the
 *                      buffer really has alpha information.
 */
int DrawRealPatch(byte *buffer, int texwidth, int texheight, lumppatch_t *patch,
                  int origx, int origy, boolean maskZero,
                  unsigned char *transtable, boolean checkForAlpha)
{
    int         count, col = 0;
    int         x = origx, y, top; // Keep track of pos for clipping.
    int         w = SHORT(patch->width);
    int         bufsize = texwidth * texheight;
    column_t   *column;
    byte       *destTop = buffer + origx;
    byte       *destAlphaTop = buffer + origx + bufsize;
    byte       *dest1, *dest2;
    byte       *source;

    for(; col < w; ++col, destTop++, destAlphaTop++, ++x)
    {
        column = (column_t *) ((byte *) patch + LONG(patch->columnofs[col]));
        top = -1;

        // Step through the posts in a column
        while(column->topdelta != 0xff)
        {
            source = (byte *) column + 3;

            if(x < 0 || x >= texwidth)
                break;          // Out of bounds.

            if(column->topdelta <= top)
                top += column->topdelta;
            else
                top = column->topdelta;

            if((count = column->length) > 0)
            {
                y = origy + top;
                dest1 = destTop + y * texwidth;
                dest2 = destAlphaTop + y * texwidth;

                while(count--)
                {
                    byte palidx = *source++;

                    // Do we need to make a translation?
                    if(transtable)
                        palidx = transtable[palidx];

                    // Is the destination within bounds?
                    if(y >= 0 && y < texheight)
                    {
                        if(!maskZero || palidx)
                            *dest1 = palidx;

                        if(maskZero)
                            *dest2 = (palidx ? 0xff : 0);
                        else
                            *dest2 = 0xff;
                    }

                    // One row down.
                    dest1 += texwidth;
                    dest2 += texwidth;
                    y++;
                }
            }

            column = (column_t *) ((byte *) column + column->length + 4);
        }
    }

    if(checkForAlpha)
    {
        int         i;
        boolean     allowSingleAlpha = (texwidth < 128 || texheight < 128);

        // Scan through the RGBA buffer and check for sub-0xff alpha.
        source = buffer + texwidth * texheight;
        for(i = 0, count = 0; i < texwidth * texheight; ++i)
        {
            if(source[i] < 0xff)
            {
                //----- <HACK> -----
                // 'Small' textures tolerate no alpha.
                if(allowSingleAlpha)
                    return true;

                // Big ones can have a single alpha pixel (ZZZFACE3!).
                if(count++ > 1)
                    return true;    // Has alpha data.
                //----- </HACK> -----
            }
        }
    }

    return false; // Doesn't have alpha data.
}

/**
 * Translate colors in the specified patch.
 */
void TranslatePatch(lumppatch_t *patch, byte *transTable)
{
    int         count;
    int         col = 0;
    column_t   *column;
    byte       *source;
    int         w = SHORT(patch->width);

    for(; col < w; ++col)
    {
        column = (column_t *) ((byte *) patch + LONG(patch->columnofs[col]));

        // Step through the posts in a column
        while(column->topdelta != 0xff)
        {
            source = (byte *) column + 3;
            count = column->length;
            while(count--)
            {
                *source = transTable[*source];
                source++;
            }
            column = (column_t *) ((byte *) column + column->length + 4);
        }
    }
}

/**
 * Converts the image data to grayscale luminance in-place.
 */
void GL_ConvertToLuminance(image_t *image)
{
    int         p, total = image->width * image->height;
    byte       *ptr = image->pixels;

    if(image->pixelSize == 1)
    {
        // No need to convert anything.
        return;
    }

    // Average the RGB colors.
    for(p = 0; p < total; ++p, ptr += image->pixelSize)
    {
        image->pixels[p] = (ptr[0] + ptr[1] + ptr[2]) / 3;
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
    return 1;                   // Successful.
}

/**
 * The imgdata must have alpha info, too.
 */
void ImageAverageRGB(byte *imgdata, int width, int height, byte *rgb,
                     byte *palette)
{
    int         i, c, integerRGB[3] = { 0, 0, 0 }, count = 0;

    for(i = 0; i < height; ++i)
    {
        if(LineAverageRGB(imgdata, width, height, i, rgb, palette, true))
        {
            count++;
            for(c = 0; c < 3; ++c)
                integerRGB[c] += rgb[c];
        }
    }

    if(count)                   // If there were pixels...
        for(c = 0; c < 3; ++c)
            rgb[c] = integerRGB[c] / count;
}

/**
 * Fills the empty pixels with reasonable color indices in order to get rid
 * of black outlines caused by texture filtering.
 *
 * FIXME: Not a very efficient algorithm...
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
 * Desaturates the texture in the dest buffer by averaging the colour then
 * looking up the nearest match in the PLAYPAL. Increases the brightness
 * to maximum.
 */
void DeSaturate(byte *buffer, byte *palette, int width, int height)
{
    int         i, max, temp = 0, paletteIndex = 0;
    byte       *rgb, *pal18to8 = GL_GetPal18to8();
    const int   numpels = width * height;

    // What is the maximum color value?
    max = 0;
    for(i = 0; i < numpels; ++i)
    {
        rgb = &palette[buffer[i] * 3];
        temp = (2 * (int)rgb[0] + 4 * (int)rgb[1] + 3 * (int)rgb[2]) / 9;
        if(temp > max)
        {
            max = temp;
        }
    }

    for(i = 0; i < numpels; ++i)
    {
        rgb = &palette[buffer[i] * 3];

        // Calculate a weighted average.
        temp = (2 * (int)rgb[0] + 4 * (int)rgb[1] + 3 * (int)rgb[2]) / 9;
        if(max)
        {
            temp *= 255.f / max;
        }

        paletteIndex = pal18to8[ RGB18(temp >> 2, temp >> 2, temp >> 2) ];
        buffer[i] = paletteIndex;
    }
}

/**
 * The given RGB color is scaled uniformly so that the highest component
 * becomes one.
 */
static void amplify(float *rgb)
{
    int         i;
    float       max = 0;

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
 * Used by flares and dynamic lights. The resulting average color is
 * amplified to be as bright as possible.
 */
void averageColorIdx(rgbcol_t *col, byte *data, int w, int h, byte *palette,
                     boolean hasAlpha)
{
    int         i;
    uint        count;
    const int   numpels = w * h;
    byte       *alphaStart = data + numpels, rgb[3];
    float       r, g, b;

    // First clear them.
    for(i = 0; i < 3; ++i)
        col->rgb[i] = 0;

    r = g = b = count = 0;
    for(i = 0; i < numpels; ++i)
    {
        if(!hasAlpha || alphaStart[i])
        {
            count++;
            memcpy(rgb, palette + 3 * data[i], 3);
            r += rgb[0] / 255.f;
            g += rgb[1] / 255.f;
            b += rgb[2] / 255.f;
        }
    }
    if(!count)
        return; // Line added by GMJ 22/07/01

    col->rgb[0] = r / count;
    col->rgb[1] = g / count;
    col->rgb[2] = b / count;

    // Make it glow (average colors are used with flares and dynlights).
    amplify(col->rgb);
}

void averageColorRGB(rgbcol_t *col, byte *data, int w, int h)
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
        col->rgb[i] = cumul[i] / numpels;

    amplify(col->rgb);
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

    // TODO: This is not very efficent.
    // Better to use an algorithm which works on full rows and full columns.
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
 * casts.
 * 2005-02-22 (danij): Modified to handle sprites with large areas of alpha
 *                     by only working on the region of the buffer that contains
 *                     non-alpha pixels (crop).
 * 2003-05-30 (skyjake): Modified to handle pixel sizes 1 (==2), 3 and 4.
 */
void GL_CalcLuminance(int pnum, byte *buffer, int width, int height,
                      int pixelsize)
{
    byte   *palette = pixelsize == 1 ? GL_GetPalette() : NULL;
    spritelump_t *slump = spritelumps[pnum];
    int     i, k, x, y, c, cnt = 0, poscnt = 0;
    byte    rgb[3], *src, *alphasrc = NULL;
    int     limit = 0xc0, poslimit = 0xe0, collimit = 0xc0;
    int     avcnt = 0, lowcnt = 0;
    rgbcol_t *sprcol = &slump->color;
    float   average[3], lowavg[3];
    int     region[4];

    for(i = 0; i < 3; ++i)
    {
        average[i] = 0;
        lowavg[i] = 0;
    }
    src = buffer;

    if(pixelsize == 1)
    {
        // In paletted mode, the alpha channel follows the actual image.
        alphasrc = buffer + width * height;
    }

    GL_GetNonAlphaRegion(buffer, width, height, pixelsize, &region[0]);
    if(region[2] > 0)
    {
        src += pixelsize * width * region[2];
        alphasrc += width * region[2];
    }
    slump->flarex = slump->flarey = 0;

    for(k = region[2], y = 0; k < region[3] + 1; ++k, ++y)
    {
        if(region[0] > 0)
        {
            src += pixelsize * region[0];
            alphasrc += region[0];
        }

        for(i = region[0], x = 0; i < region[1] + 1; ++i, ++x, src += pixelsize, alphasrc++)
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

            // Bright enough?
            if(pixelsize == 1)
            {
                memcpy(rgb, palette + (*src * 3), 3);
            }
            else if(pixelsize >= 3)
            {
                memcpy(rgb, src, 3);
            }

            if(rgb[0] > poslimit || rgb[1] > poslimit || rgb[2] > poslimit)
            {
                // This pixel will participate in calculating the average
                // center point.
                slump->flarex += x;
                slump->flarey += y;
                poscnt++;
            }

            // Bright enough to affect size?
            if(rgb[0] > limit || rgb[1] > limit || rgb[2] > limit)
                cnt++;

            // How about the color of the light?
            if(rgb[0] > collimit || rgb[1] > collimit || rgb[2] > collimit)
            {
                avcnt++;
                for(c = 0; c < 3; ++c)
                    average[c] += rgb[c] / 255.f;
            }
            else
            {
                lowcnt++;
                for(c = 0; c < 3; ++c)
                    lowavg[c] += rgb[c] / 255.f;
            }
        }
        if(region[1] < width - 1)
        {
            src += pixelsize * (width - 1 - region[1]);
            alphasrc += (width - 1 - region[1]);
        }
    }
    if(!poscnt)
    {
        slump->flarex = region[0] + ((region[1] - region[0]) / 2.0f);
        slump->flarey = region[2] + ((region[3] - region[2]) / 2.0f);
    }
    else
    {
        // Get the average.
        slump->flarex /= poscnt;
        slump->flarey /= poscnt;
        // Add the origin offset.
        slump->flarex += region[0];
        slump->flarey += region[2];
    }

    // The color.
    if(!avcnt)
    {
        if(!lowcnt)
        {
            // Doesn't the thing have any pixels??? Use white light.
            for(c = 0; c < 3; ++c)
                sprcol->rgb[c] = 1;
        }
        else
        {
            // Low-intensity color average.
            for(c = 0; c < 3; ++c)
                sprcol->rgb[c] = lowavg[c] / lowcnt;
        }
    }
    else
    {
        // High-intensity color average.
        for(c = 0; c < 3; ++c)
            sprcol->rgb[c] = average[c] / avcnt;
    }

#ifdef _DEBUG
    VERBOSE2( Con_Message("GL_CalcLuminance: Proc \"%s\"\n"
                          "  width %dpx, height %dpx, bits %d\n"
                          "  cell region X[%d, %d] Y[%d, %d]\n"
                          "  flare X= %g Y=%g %s\n"
                          "  flare RGB[%g, %g, %g] %s\n",
                          W_CacheLumpNum(slump->lump, PU_GETNAME),
                          width, height, pixelsize,
                          region[0], region[1], region[2], region[3],
                          slump->flarex, slump->flarey,
                          (poscnt? "(average)" : "(center)"),
                          sprcol->rgb[0], sprcol->rgb[1], sprcol->rgb[2],
                          (avcnt? "(hi-intensity avg)" :
                           lowcnt? "(low-intensity avg)" : "(white light)")) );
#endif

    // Amplify color.
    amplify(sprcol->rgb);
    // How about the size of the light source?
    slump->lumsize = (2 * cnt + avcnt) / 3.0f / 70.0f;
    if(slump->lumsize > 1)
        slump->lumsize = 1;
}

/**
 * @return          <code>true</code> if the given color is either
 *                  (0,255,255) or (255,0,255).
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
    uint        i;

    for(i = 0; i < width; ++i, rgbaBuf += 4)
        if(ColorKey(rgbaBuf))
            rgbaBuf[3] = rgbaBuf[2] = rgbaBuf[1] = rgbaBuf[0] = 0;
}

/**
 * Take the input buffer and convert to color keyed. A new buffer may
 * be needed if the input buffer has three color components.
 *
 * @return          If the in buffer wasn't large enough will return a ptr
 *                  to the newly allocated buffer which must be freed with
 *                  M_Free().
 */
byte *GL_ApplyColorKeying(byte *buf, uint pixelSize, uint width,
                          uint height)
{
    uint        i;
    byte       *ckdest, *in, *out;
    const uint  numpels = width * height;

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
