/** @file tga.cpp  Truevision TGA (a.k.a Targa) image reader
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2009-2013 Daniel Swanson <danij@dengine.net>
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

#include "doomsday/resource/tga.h"
#include "dd_share.h"

#include <de/memory.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace de;

enum {
    TGA_FALSE,
    TGA_TRUE,
    TGA_TARGA24, // rgb888
    TGA_TARGA32 // rgba8888
};

#pragma pack(1)
typedef struct {
    uint8_t idLength; // Identification field size in bytes.
    uint8_t colorMapType; // Type of the color map.
    uint8_t imageType; // Image type code.
} tga_header_t;

// Color map specification.
typedef struct {
    int16_t index; // Index of first color map entry.
    int16_t length; // Number of color map entries.
    uint8_t entrySize; // Number of bits in a color map entry (16/24/32).
} tga_colormapspec_t;

// Image specification flags:
#define ISF_SCREEN_ORIGIN_UPPER 0x1 // Upper left-hand corner screen origin.
// Data interleaving:
#define ISF_INTERLEAVE_TWOWAY   0x2 // Two-way (even/odd) interleaving.
#define ISF_INTERLEAVE_FOURWAY  0x4 // Four-way interleaving.

// Image specification.
typedef struct {
    uint8_t flags;
    int16_t xOrigin; // X coordinate of lower left corner.
    int16_t yOrigin; // Y coordinate of lower left corner.
    int16_t width; // Width of the image in pixels.
    int16_t height; // Height of the image in pixels.
    uint8_t pixelDepth; // Number of bits in a pixel (16/24/32).
    uint8_t attributeBits;
} tga_imagespec_t;
#pragma pack()

static char *lastTgaErrorMsg = 0; /// @todo potentially never free'd

static void TGA_SetLastError(char const *msg)
{
    size_t len;
    if (0 == msg || 0 == (len = strlen(msg)))
    {
        if (lastTgaErrorMsg != 0)
        {
            M_Free(lastTgaErrorMsg);
        }
        lastTgaErrorMsg = 0;
        return;
    }

    lastTgaErrorMsg = (char *) M_Realloc(lastTgaErrorMsg, len+1);
    strcpy(lastTgaErrorMsg, msg);
}

static uint8_t readByte(FileHandle &f)
{
    uint8_t v;
    f.read(&v, 1);
    return v;
}

static int16_t readShort(FileHandle &f)
{
    int16_t v;
    f.read((uint8_t *)&v, sizeof(v));
    return DD_SHORT(v);
}

static void readHeader(tga_header_t *dst, FileHandle &file)
{
    dst->idLength     = readByte(file);
    dst->colorMapType = readByte(file);
    dst->imageType    = readByte(file);
}

static void readColorMapSpec(tga_colormapspec_t *dst, FileHandle &file)
{
    dst->index     = readShort(file);
    dst->length    = readShort(file);
    dst->entrySize = readByte(file);
}

static void readImageSpec(tga_imagespec_t *dst, FileHandle &file)
{
    dst->xOrigin    = readShort(file);
    dst->yOrigin    = readShort(file);
    dst->width      = readShort(file);
    dst->height     = readShort(file);
    dst->pixelDepth = readByte(file);

    uint8_t bits = readByte(file);

    /*
     * attributeBits:4; // Attribute bits associated with each pixel.
     * reserved:1; // A reserved bit; must be 0.
     * screenOrigin:1; // Location of screen origin; must be 0.
     * dataInterleave:2; // TGA_INTERLEAVE_*
     */
    dst->flags = 0 | ((bits & 6)? ISF_SCREEN_ORIGIN_UPPER : 0) |
        ((bits & 7)? ISF_INTERLEAVE_TWOWAY : (bits & 8)? ISF_INTERLEAVE_FOURWAY : 0);
    dst->attributeBits = (bits & 0xf);
}

const char* TGA_LastError(void)
{
    if (lastTgaErrorMsg)
        return lastTgaErrorMsg;
    return 0;
}

uint8_t *TGA_Load(FileHandle &file, Vector2ui &outSize, int &pixelSize)
{
    uint8_t *dstBuf = 0;

    size_t const initPos = file.tell();

    tga_header_t header;
    readHeader(&header, file);

    tga_colormapspec_t colorMapSpec;
    readColorMapSpec(&colorMapSpec, file);

    tga_imagespec_t imageSpec;
    readImageSpec(&imageSpec, file);

    outSize = Vector2ui(imageSpec.width, imageSpec.height);

    if (header.imageType != 2 ||
       (imageSpec.pixelDepth != 32 && imageSpec.pixelDepth != 24) ||
       (imageSpec.attributeBits != 8 &&
        imageSpec.attributeBits != 0) ||
        (imageSpec.flags & ISF_SCREEN_ORIGIN_UPPER))
    {
        TGA_SetLastError("Unsupported format.");
        file.seek(initPos, SeekSet);
        return 0;
    }

    // Determine format.
    //int const format = (imageSpec.pixelDepth == 24? TGA_TARGA24 : TGA_TARGA32);

    pixelSize = (imageSpec.pixelDepth == 24? 3 : 4);

    // Read the pixel data.
    int const numPels = outSize.x * outSize.y;
    uint8_t *srcBuf = (uint8_t *) M_Malloc(numPels * pixelSize);
    file.read(srcBuf, numPels * pixelSize);

    // "Unpack" the pixels (origin in the lower left corner).
    // TGA pixels are in BGRA format.
    dstBuf = (uint8_t *) M_Malloc(4 * numPels);
    uint8_t const *src = srcBuf;
    for (int y = outSize.y - 1; y >= 0; y--)
    for (int x = 0; x < (signed) outSize.x; x++)
    {
        uint8_t *dst = &dstBuf[(y * outSize.x + x) * pixelSize];

        dst[2] = *src++;
        dst[1] = *src++;
        dst[0] = *src++;
        if (pixelSize == 4) dst[3] = *src++;
    }
    M_Free(srcBuf);

    TGA_SetLastError(0); // Success.
    file.seek(initPos, SeekSet);

    return dstBuf;
}
