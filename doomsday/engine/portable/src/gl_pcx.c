/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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

/**
 * gl_pcx.c: PCX Images
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_graphics.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

#pragma pack(1)
typedef struct {
    char            manufacturer;
    char            version;
    char            encoding;
    char            bits_per_pixel;
    unsigned short  xmin, ymin, xmax, ymax;
    unsigned short  hres, vres;
    unsigned char   palette[48];
    char            reserved;
    char            color_planes;
    unsigned short  bytes_per_line;
    unsigned short  palette_type;
    char            filler[58];
    unsigned char   data;          // unbounded
} pcx_t;
#pragma pack()

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static boolean memoryLoad(const byte* imgdata, size_t len, int bufW,
                          int bufH, byte* outBuffer)
{
    const pcx_t*        pcx = (const pcx_t*) imgdata;
    const byte*         raw = &pcx->data;
    byte*               palette, *pix;
    int                 x, y, dataByte, runLength;
    short               xmax = SHORT(pcx->xmax);
    short               ymax = SHORT(pcx->ymax);

    if(!outBuffer)
        return false;

    // Check the format.
    if(pcx->manufacturer != 0x0a || pcx->version != 5 ||
       pcx->encoding != 1 || pcx->bits_per_pixel != 8)
    {
        //Con_Message("PCX_Load: unsupported format.\n");
        return false;
    }

    // Check that the PCX is not larger than the buffer.
    if(xmax >= bufW || ymax >= bufH)
    {
        Con_Message("PCX_Load: larger than expected.\n");
        return false;
    }

    palette = M_Malloc(768);
    memcpy(palette, ((byte*) pcx) + len - 768, 768); // Palette is in the end.

    pix = outBuffer;

    for(y = 0; y <= ymax; ++y, pix += (xmax + 1) * 3)
    {
        for(x = 0; x <= xmax;)
        {
            dataByte = *raw++;

            if((dataByte & 0xC0) == 0xC0)
            {
                runLength = dataByte & 0x3F;
                dataByte = *raw++;
            }
            else
                runLength = 1;

            while(runLength-- > 0)
            {
                memcpy(pix + x++ * 3, palette + dataByte * 3, 3);
            }
        }
    }

    if((size_t) (raw - (byte *) pcx) > len)
        Con_Error("PCX_Load: Corrupt image!\n");

    M_Free(palette);
    return true;
}

/**
 * PCX loader, partly borrowed from the Q2 utils source (lbmlib.c).
 */
static boolean load(const char* fn, int bufW, int bufH, byte* outBuffer)
{
    DFILE*              file;

    if((file = F_Open(fn, "rb")))
    {
        byte*               raw;
        size_t              len;

        // Load the file.
        F_Seek(file, 0, SEEK_END); // Seek to end.
        len = F_Tell(file); // How long?
        F_Seek(file, 0, SEEK_SET);
        raw = M_Malloc(len);
        F_Read(raw, len, file);
        F_Close(file);

        // Parse the PCX file.
        if(!memoryLoad(raw, len, bufW, bufH, outBuffer))
        {
            Con_Message("PCX_Load: Error loading \"%s\".\n",
                        M_PrettyPath(fn));
            outBuffer = NULL;
        }

        M_Free(raw);
        return true;
    }

    Con_Message("PCX_Load: Can't find %s.\n", fn);
    return false;
}

boolean PCX_Load(const char* fn, int bufW, int bufH, byte* outBuffer)
{
    if(!outBuffer)
        return false;

    return load(fn, bufW, bufH, outBuffer);
}

/**
 * @return              @c true, if a PCX image (probably).
 */
boolean PCX_MemoryLoad(const byte* imgdata, size_t len, int bufW, int bufH,
                       byte* outBuffer)
{
    if(memoryLoad(imgdata, len, bufW, bufH, outBuffer))
        return true;

    return false;
}

boolean PCX_GetSize(const char* fn, int* w, int* h)
{
    DFILE*              file;

    if((file = F_Open(fn, "rb")))
    {
        pcx_t               hdr;
        size_t              read;

        read = F_Read(&hdr, sizeof(hdr), file);
        F_Close(file);

        if(!(read < sizeof(hdr)))
            return PCX_MemoryGetSize(&hdr, w, h);
    }

    return false;
}

/**
 * @return              @c true, if successful.
 */
boolean PCX_MemoryGetSize(const void* imageData, int* w, int* h)
{
    const pcx_t*        hdr = (const pcx_t*) imageData;

    if(hdr->manufacturer != 0x0a || hdr->version != 5 ||
       hdr->encoding != 1 || hdr->bits_per_pixel != 8)
        return false;

    if(w)
        *w = SHORT(hdr->xmax) + 1;
    if(h)
        *h = SHORT(hdr->ymax) + 1;

    return true;
}
