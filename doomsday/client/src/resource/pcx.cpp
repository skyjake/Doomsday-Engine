/**\file pcx.cpp
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1997-2006 by id Software, Inc.
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

#include "de_console.h"
#include "de_system.h"
#include "de_filesys.h"
#include "de_graphics.h"

#include "m_misc.h"

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

static char* lastErrorMsg = 0; /// @todo potentially never free'd

static void setLastError(const char* msg)
{
    size_t len;
    if(0 == msg || 0 == (len = strlen(msg)))
    {
        if(lastErrorMsg != 0)
            free(lastErrorMsg);
        lastErrorMsg = 0;
        return;
    }

    lastErrorMsg = (char *) M_Realloc(lastErrorMsg, len+1);
    strcpy(lastErrorMsg, msg);
}

static int load(FileHandle* file, int width, int height, uint8_t* dstBuf)
{
    assert(file && dstBuf);
    {
    int x, y, dataByte, runLength;
    boolean result = false;
    const uint8_t* srcPos, *palette;
    uint8_t* raw;
    size_t len;

    len = FileHandle_Length(file);
    if(0 == (raw = (uint8_t *) M_Malloc(len)))
        Con_Error("PCX_Load: Failed on allocation of %lu bytes for read buffer.", (unsigned long) len);
    FileHandle_Read(file, raw, len);
    srcPos = raw;
    // Palette is at the end.
    palette = srcPos + len - 768;

    srcPos += sizeof(header_t);
    for(y = 0; y < height; ++y, dstBuf += width * 3)
    {
        for(x = 0; x < width;)
        {
            dataByte = *srcPos++;

            if((dataByte & 0xC0) == 0xC0)
            {
                runLength = dataByte & 0x3F;
                dataByte = *srcPos++;
            }
            else
                runLength = 1;

            while(runLength-- > 0)
            {
                memcpy(dstBuf + x++ * 3, palette + dataByte * 3, 3);
            }
        }
    }

    if(!((size_t) (srcPos - (uint8_t*) raw) > len))
    {
        setLastError(0); // Success.
        result = true;
    }
    else
    {
        setLastError("RLE inflation failed.");
    }

    free(raw);
    return result;
    }
}

const char* PCX_LastError(void)
{
    if(lastErrorMsg)
        return lastErrorMsg;
    return 0;
}

uint8_t* PCX_Load(FileHandle* file, int* width, int* height, int* pixelSize)
{
    assert(file && width && height && pixelSize);
    {
    header_t hdr;
    size_t initPos = FileHandle_Tell(file);
    size_t readBytes = FileHandle_Read(file, (uint8_t*)&hdr, sizeof(hdr));
    uint8_t* dstBuf = 0;
    if(!(readBytes < sizeof(hdr)))
    {
        size_t dstBufSize;

        if(hdr.manufacturer != 0x0a || hdr.version != 5 ||
           hdr.encoding != 1 || hdr.bits_per_pixel != 8)
        {
            setLastError("Unsupported format.");
            FileHandle_Seek(file, initPos, SeekSet);
            return 0;
        }

        *width  = SHORT(hdr.xmax) + 1;
        *height = SHORT(hdr.ymax) + 1;
        *pixelSize = 3;

        dstBufSize = 4 * (*width) * (*height);
        if(0 == (dstBuf = (uint8_t *) M_Malloc(dstBufSize)))
            Con_Error("PCX_Load: Failed on allocation of %lu bytes for output buffer.", (unsigned long) dstBufSize);

        FileHandle_Rewind(file);
        if(!load(file, *width, *height, dstBuf))
        {
            free(dstBuf);
            dstBuf = 0;
        }
    }
    FileHandle_Seek(file, initPos, SeekSet);
    return dstBuf;
    }
}
