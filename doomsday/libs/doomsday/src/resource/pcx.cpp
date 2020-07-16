/** @file pcx.cpp  PCX image reader.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1997-2006 by id Software, Inc.
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

#include "doomsday/pcx.h"
#include "dd_share.h"

#include <de/legacy/memory.h>

using namespace de;
using namespace res;

#pragma pack(1)
typedef struct {
    int8_t manufacturer;
    int8_t version;
    int8_t encoding;
    int8_t bits_per_pixel;
    uint16_t xmin, ymin, xmax, ymax;
    uint16_t hres, vres;
    uint8_t palette[48];
    int8_t reserved;
    int8_t color_planes;
    uint16_t bytes_per_line;
    uint16_t palette_type;
    int8_t filler[58];
} header_t;
#pragma pack()

static char *lastPcxErrorMsg = 0; /// @todo potentially never free'd

static void PCX_SetLastError(const char *msg)
{
    size_t len;
    if (0 == msg || 0 == (len = strlen(msg)))
    {
        if (lastPcxErrorMsg != 0)
        {
            M_Free(lastPcxErrorMsg);
        }
        lastPcxErrorMsg = 0;
        return;
    }

    lastPcxErrorMsg = (char *) M_Realloc(lastPcxErrorMsg, len+1);
    strcpy(lastPcxErrorMsg, msg);
}

static bool load(FileHandle &file, int width, int height, uint8_t *dstBuf)
{
    DE_ASSERT(dstBuf != 0);

    int x, y, dataByte, runLength;
    bool result = false;

    const size_t len = file.length();
    uint8_t *raw = (uint8_t *) M_Malloc(len);

    file.read(raw, len);

    const uint8_t *srcPos = raw;
    const uint8_t *palette = srcPos + len - 768; // Palette is at the end.

    srcPos += sizeof(header_t);
    for (y = 0; y < height; ++y, dstBuf += width * 3)
    {
        for (x = 0; x < width;)
        {
            dataByte = *srcPos++;

            if ((dataByte & 0xC0) == 0xC0)
            {
                runLength = dataByte & 0x3F;
                dataByte = *srcPos++;
            }
            else
            {
                runLength = 1;
            }

            while (runLength-- > 0)
            {
                std::memcpy(dstBuf + x++ * 3, palette + dataByte * 3, 3);
            }
        }
    }

    if (!((size_t) (srcPos - (uint8_t *) raw) > len))
    {
        PCX_SetLastError(0); // Success.
        result = true;
    }
    else
    {
        PCX_SetLastError("RLE inflation failed.");
    }

    M_Free(raw);
    return result;
}

const char *PCX_LastError()
{
    if (lastPcxErrorMsg)
    {
        return lastPcxErrorMsg;
    }
    return 0;
}

uint8_t *PCX_Load(FileHandle &file, de::Vec2ui &outSize, int &pixelSize)
{
    uint8_t *dstBuf = 0;

    const size_t initPos = file.tell();

    header_t hdr;
    if (file.read((uint8_t *)&hdr, sizeof(hdr)) >= sizeof(hdr))
    {
        size_t dstBufSize;

        if (hdr.manufacturer != 0x0a || hdr.version != 5 ||
           hdr.encoding != 1 || hdr.bits_per_pixel != 8)
        {
            PCX_SetLastError("Unsupported format.");
            file.seek(initPos, SeekSet);
            return 0;
        }

        outSize   = Vec2ui(DD_SHORT(hdr.xmax) + 1, DD_SHORT(hdr.ymax) + 1);
        pixelSize = 3;

        dstBufSize = 4 * outSize.x * outSize.y;
        dstBuf = (uint8_t *) M_Malloc(dstBufSize);

        file.rewind();
        if (!load(file, outSize.x, outSize.y, dstBuf))
        {
            M_Free(dstBuf);
            dstBuf = 0;
        }
    }
    file.seek(initPos, SeekSet);
    return dstBuf;
}
