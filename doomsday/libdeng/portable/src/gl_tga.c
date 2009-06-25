/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2009 Daniel Swanson <danij@dengine.net>
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
 * gl_tga.c: TGA file format (TARGA) reader/writer.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "de_base.h"
#include "de_system.h"
#include "de_graphics.h"

// MACROS ------------------------------------------------------------------

#undef SHORT
#ifdef __BIG_ENDIAN__
#define SHORT(x)            shortSwap(x)
# else // Little-endian.
#define SHORT(x)            (x)
#endif

// TYPES -------------------------------------------------------------------

typedef unsigned char uchar; // 1 byte

typedef struct {
    uchar           idLength; // Identification field size in bytes.
    uchar           colorMapType; // Type of the color map.
    uchar           imageType; // Image type code.
} tga_header_t;

// Color map specification.
typedef struct {
    int16_t         index; // Index of first color map entry.
    int16_t         length; // Number of color map entries.
    uchar           entrySize; // Number of bits in a color map entry (16/24/32).
} tga_colormapspec_t;

// Image specification flags:
#define ISF_SCREEN_ORIGIN_UPPER 0x1 // Upper left-hand corner screen origin.
// Data interleaving:
#define ISF_INTERLEAVE_TWOWAY   0x2 // Two-way (even/odd) interleaving.
#define ISF_INTERLEAVE_FOURWAY  0x4 // Four-way interleaving.

// Image specification.
typedef struct {
    uchar           flags;
    int16_t         xOrigin; // X coordinate of lower left corner.
    int16_t         yOrigin; // Y coordinate of lower left corner.
    int16_t         width; // Width of the image in pixels.
    int16_t         height; // Height of the image in pixels.
    uchar           pixelDepth; // Number of bits in a pixel (16/24/32).
    uchar           attributeBits;
} tga_imagespec_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

#ifdef __BIG_ENDIAN__
static int16_t shortSwap(int16_t n)
{
    return ((n & 0xff) << 8) | ((n & 0xff00) >> 8);
}
#endif

static void writeByte(FILE* f, uchar b)
{
    fwrite(&b, 1, 1, f);
}

static void writeShort(FILE* f, int16_t s)
{
    int16_t             v = SHORT(s);
    fwrite(&v, sizeof(v), 1, f);
}

static uchar readByte(DFILE* f)
{
    uchar               v;
    F_Read(&v, 1, f);
    return v;
}

static int16_t readShort(DFILE* f)
{
    int16_t             v;
    F_Read(&v, sizeof(v), f);
    return SHORT(v);
}

/**
 * @param idLength      Identification field size in bytes (max 255).
 *                      @c 0 indicates no identification field.
 * @param colorMapType  Type of the color map, @c 0 or @c 1:
 *                      @c 0 = color map data is not present.
 *                      @c 1 = color map data IS present.
 * @param imageType     Image data type code, one of:
 *                      @c 0 = no image data is present.
 *                      @c 1 = uncompressed, color mapped image.
 *                      @c 2 = uncompressed, true-color image.
 *                      @c 3 = uncompressed, grayscale image.
 *                      @c 9 = run-length encoded, color mapped image.
 *                      @c 10 = run-length encoded, true-color image.
 *                      @c 11 = run-length encoded, grayscale image.
 * @param file          Handle to the file to be written to.
 */
static void writeHeader(uchar idLength, uchar colorMapType, uchar imageType,
                        FILE* file)
{
    writeByte(file, idLength);
    writeByte(file, colorMapType? 1 : 0);
    writeByte(file, imageType);
}

static void readHeader(tga_header_t* dst, DFILE* file)
{
    dst->idLength = readByte(file);
    dst->colorMapType = readByte(file);
    dst->imageType = readByte(file);
}

/**
 * @param index         Index of first color map entry.
 * @param length        Total number of color map entries.
 * @param entrySize     Number of bits in a color map entry; 15/16/24/32.
 * @param file          Handle to the file to be written to.
 */
static void writeColorMapSpec(int16_t index, int16_t length,
                              uchar entrySize, FILE* file)
{
    writeShort(file, index);
    writeShort(file, length);
    writeByte(file, entrySize);
}

static void readColorMapSpec(tga_colormapspec_t* dst, DFILE* file)
{
    dst->index = readShort(file);
    dst->length = readShort(file);
    dst->entrySize = readByte(file);
}

/**
 * @param xOrigin       X coordinate of lower left corner.
 * @param yOrigin       Y coordinate of lower left corner.
 * @param width         Width of the image in pixels.
 * @param height        Height of the image in pixels.
 * @param pixDepth      Number of bits per pixel, one of; 16/24/32.
 * @param file          Handle to the file to be written to.
 */
static void writeImageSpec(int16_t xOrigin, int16_t yOrigin,
                           int16_t width, int16_t height, uchar pixDepth,
                           FILE* file)
{
    writeShort(file, xOrigin);
    writeShort(file, yOrigin);
    writeShort(file, width);
    writeShort(file, height);
    writeByte(file, pixDepth);

    /**
     * attributeBits:4; // Attribute bits associated with each pixel.
     * reserved:1; // A reserved bit; must be 0.
     * screenOrigin:1; // Location of screen origin; must be 0.
     * dataInterleave:2; // TGA_INTERLEAVE_*
     */
    writeByte(file, 0);
}

static void readImageSpec(tga_imagespec_t* dst, DFILE* file)
{
    uchar               bits;

    dst->xOrigin = readShort(file);
    dst->yOrigin = readShort(file);
    dst->width = readShort(file);
    dst->height = readShort(file);
    dst->pixelDepth = readByte(file);
    bits = readByte(file);

    /**
     * attributeBits:4; // Attribute bits associated with each pixel.
     * reserved:1; // A reserved bit; must be 0.
     * screenOrigin:1; // Location of screen origin; must be 0.
     * dataInterleave:2; // TGA_INTERLEAVE_*
     */
    dst->flags = 0 | ((bits & 6)? ISF_SCREEN_ORIGIN_UPPER : 0) |
        ((bits & 7)? ISF_INTERLEAVE_TWOWAY : (bits & 8)? ISF_INTERLEAVE_FOURWAY : 0);
    dst->attributeBits = (bits & 0xf);
}

/**
 * Saves the buffer (which is formatted rgb565) to a Targa 24 image file.
 *
 * @param filename      Path to the file to be written to (need not exist).
 * @param w             Width of the image in pixels.
 * @param h             Height of the image in pixels.
 * @param buf           Ptr to the image data to be written.
 *
 * @return              Non-zero iff successful.
 */
int TGA_Save24_rgb565(const char* filename, int w, int h,
                      const uint16_t* buf)
{
    int                 i, k;
    FILE*               file;
    uchar*              outBuf;
    size_t              outBufStart;

    if(!buf || !(w > 0 && h > 0))
        return 0;

    if((file = fopen(filename, "wb")) == NULL)
        return 0;

    // No identification field, no color map, Targa type 2 (unmapped RGB).
    writeHeader(0, 0, 2, file);
    writeColorMapSpec(0, 0, 0, file);
    writeImageSpec(0, 0, w, h, 24, file);

    /**
     * Convert the buffer:
     * From RRRRRGGGGGGBBBBB > ARRRRRGGGGGBBBBB
     *
     * \note Alpha will always be @c 0.
     */
    outBuf = malloc(w * h * 3);
    outBufStart = w * h - 1; // From the end.
    for(k = 0; k < h; ++k)
        for(i = 0; i < w; ++i)
        {
            int16_t            r, g, b;
            const int16_t      src = buf[k * w + i];
            uchar*              dst = &outBuf[outBufStart - (((k + 1) * w - 1 - i)) * 3];

            r = (src >> 11) & 0x1f; // The last 5 bits.
            g = (src >> 5) & 0x3f; // The middle 6 bits (one bit'll be lost).
            b = src & 0x1f; // The first 5 bits.

            dst[2] = b << 3;
            dst[0] = g << 2;
            dst[1] = r << 3;
        }
    // Write the converted buffer (bytes may go the wrong way around...!).
    fwrite(outBuf, w * h * 3, 1, file);
    free(outBuf);

    fclose(file);

    return 1; // Success.
}

/**
 * Save the rgb888 buffer as Targa 24.
 *
 * @param filename      Path to the file to be written to (need not exist).
 * @param w             Width of the image in pixels.
 * @param h             Height of the image in pixels.
 * @param buf           Ptr to the image data to be written.
 *
 * @return              Non-zero iff successful.
 */
int TGA_Save24_rgb888(const char* filename, int w, int h, const byte* buf)
{
    int                 i;
    FILE*               file;
    uchar*              outBuf;

    if(!buf || !(w > 0 && h > 0))
        return 0;

    if((file = fopen(filename, "wb")) == NULL)
        return 0; // Huh?

    // No identification field, no color map, Targa type 2 (unmapped RGB).
    writeHeader(0, 0, 2, file);
    writeColorMapSpec(0, 0, 0, file);
    writeImageSpec(0, 0, w, h, 24, file);

    // The save format is BRG.
    outBuf = malloc(w * h * 3);
    for(i = 0; i < w * h; ++i)
    {
        const uchar*        src = &buf[i * 3];
        uchar*              dst = &outBuf[i * 3];

        dst[0] = src[2];
        dst[1] = src[1];
        dst[2] = src[0];
    }
    fwrite(outBuf, w * h * 3, 1, file);
    free(outBuf);

    fclose(file);

    return 1; // Success.
}

/**
 * Save the rgb8888 buffer as Targa 24.
 *
 * @param filename      Path to the file to be written to (need not exist).
 * @param w             Width of the image in pixels.
 * @param h             Height of the image in pixels.
 * @param buf           Ptr to the image data to be written.
 *
 * @return              Non-zero iff successful.
 */
int TGA_Save24_rgba8888(const char* filename, int w, int h, const byte* buf)
{
    int                 i;
    FILE*               file;
    uchar*              outBuf;

    if(!buf || !(w > 0 && h > 0))
        return 0;

    if((file = fopen(filename, "wb")) == NULL)
        return 0; // Huh?

    // No identification field, no color map, Targa type 2 (unmapped RGB).
    writeHeader(0, 0, 2, file);
    writeColorMapSpec(0, 0, 0, file);
    writeImageSpec(0, 0, w, h, 24, file);

    // The save format is BGR.
    outBuf = malloc(w * h * 3);
    for(i = 0; i < w * h; ++i)
    {
        const byte*         src = &buf[i * 4];
        uchar*              dst = &outBuf[i * 3];

        dst[0] = src[2];
        dst[1] = src[1];
        dst[2] = src[0];
    }
    fwrite(outBuf, w * h * 3, 1, file);
    free(outBuf);

    fclose(file);

    return 1; // Success.
}

/**
 * Save the rgb888 buffer as Targa 16.
 *
 * @param filename      Path to the file to be written to (need not exist).
 * @param w             Width of the image in pixels.
 * @param h             Height of the image in pixels.
 * @param buf           Ptr to the image data to be written.
 *
 * @return              Non-zero iff successful.
 */
int TGA_Save16_rgb888(const char* filename, int w, int h, const byte* buf)
{
    int                 i;
    FILE*               file;
    int16_t*           outBuf;

    if(!buf || !(w > 0 && h > 0))
        return 0;

    if((file = fopen(filename, "wb")) == NULL)
        return 0; // Huh?

    // No identification field, no color map, Targa type 2 (unmapped RGB).
    writeHeader(0, 0, 2, file);
    writeColorMapSpec(0, 0, 0, file);
    writeImageSpec(0, 0, w, h, 16, file);

    // The destination format is _RRRRRGG GGGBBBBB.
    outBuf = malloc(w * h * 2);
    for(i = 0; i < w * h; ++i)
    {
        const uchar*        src = &buf[i * 3];
        int16_t*           dst = &outBuf[i];

        *dst = (src[2] >> 3) + ((src[1] & 0xf8) << 2) + ((src[0] & 0xf8) << 7);
    }
    fwrite(outBuf, w * h * 2, 1, file);
    free(outBuf);

    fclose(file);

    return 1; // Success.
}

/**
 * Loads a 24-bit or a 32-bit TGA image (24-bit color + 8-bit alpha).
 *
 * \warning: This is not a generic TGA loader. Only type 2, 24/32 pixel
 *     size, attrbits 0/8 and lower left origin supported.
 *
 * @param buf           Caller allocated memory for 'buffer', MUST be at
 *                      least 4 * w * h!
 *
 * @return              Non-zero iff the image is loaded successfully.
 */
int TGA_Load32_rgba8888(DFILE* file, int w, int h, byte* buf)
{
    int                 x, y, pixbytes, format;
    tga_header_t        header;
    tga_colormapspec_t  colorMapSpec;
    tga_imagespec_t     imageSpec;
    uchar*              srcBuf;
    const uchar*        src;

    // Read and check the header.
    readHeader(&header, file);
    readColorMapSpec(&colorMapSpec, file);
    readImageSpec(&imageSpec, file);

    if(header.imageType != 2 ||
       (imageSpec.pixelDepth != 32 && imageSpec.pixelDepth != 24) ||
       (imageSpec.attributeBits != 8 &&
        imageSpec.attributeBits != 0) ||
        (imageSpec.flags & ISF_SCREEN_ORIGIN_UPPER))
    {
        // May or may not get displayed...
        Con_Message("TGA_Load32_rgba8888: I don't know this format!\n");
        Con_Message("  (type=%i pxsize=%i abits=%i)\n", header.imageType,
                   imageSpec.pixelDepth,
                   imageSpec.attributeBits);
        return TGA_FALSE;
    }

    VERBOSE(
        Con_Message("(TGA: type=%i pxsize=%i abits=%i)\n", header.imageType,
               imageSpec.pixelDepth, imageSpec.attributeBits));

    // Determine format.
    if(imageSpec.pixelDepth == 24)
    {
        format = TGA_TARGA24;
        pixbytes = 3;
    }
    else
    {
        format = TGA_TARGA32;
        pixbytes = 4;
    }

    // Read the pixel data.
    srcBuf = malloc(w * h * pixbytes);
    F_Read(srcBuf, w * h * pixbytes, file);

    // "Unpack" the pixels (origin in the lower left corner).
    // TGA pixels are in BGRA format.
    src = srcBuf;
    for(y = h - 1; y >= 0; y--)
        for(x = 0; x < w; x++)
        {
            byte*               dst = &buf[(y * w + x) * pixbytes];

            dst[2] = *src++;
            dst[1] = *src++;
            dst[0] = *src++;
            if(pixbytes == 4)
                dst[3] = *src++;
        }
    free(srcBuf);

    // Success!
    return format;
}

/**
 * @return              @c true, iff a file was found and then read.
 */
int TGA_GetSize(const char* filename, int* w, int *h)
{
    tga_header_t        header;
    tga_colormapspec_t  colorMapSpec;
    tga_imagespec_t     imageSpec;
    DFILE*              file = F_Open(filename, "rb");

    if(!file || !w || !h)
        return 0;

    readHeader(&header, file);
    readColorMapSpec(&colorMapSpec, file);
    readImageSpec(&imageSpec, file);
    F_Close(file);

    if(w) *w = imageSpec.width;
    if(h) *h = imageSpec.height;

    return 1; // Success.
}
