/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2007 Daniel Swanson <danij@dengine.net>
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

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * @return              @c true, if successful.
 */
boolean PCX_MemoryGetSize(void *imageData, int *w, int *h)
{
    pcx_t          *hdr = (pcx_t *) imageData;

    if(hdr->manufacturer != 0x0a || hdr->version != 5 || hdr->encoding != 1 ||
       hdr->bits_per_pixel != 8)
        return false;
    if(w)
        *w = SHORT(hdr->xmax) + 1;
    if(h)
        *h = SHORT(hdr->ymax) + 1;
    return true;
}

boolean PCX_GetSize(const char *fn, int *w, int *h)
{
    DFILE          *file = F_Open(fn, "rb");
    pcx_t           hdr;

    if(!file)
        return false;

    F_Read(&hdr, sizeof(hdr), file);
    F_Close(file);

    return PCX_MemoryGetSize(&hdr, w, h);
}

/**
 * @return              @c true, if a PCX image (probably).
 */
boolean PCX_MemoryLoad(byte *imgdata, size_t len, int buf_w, int buf_h,
                       byte *outBuffer)
{
    if(PCX_MemoryAllocLoad(imgdata, len, &buf_w, &buf_h, outBuffer))
        return true;

    return false;
}

/**
 * \note Memory is allocated on the stack and must be free'd with M_Free()
 *
 * @param outBuffer     If @c NULL,, a new buffer is allocated.
 *
 * @return              Ptr to a buffer containing the loaded texture data.
 */
byte *PCX_MemoryAllocLoad(byte *imgdata, size_t len, int *buf_w, int *buf_h,
                          byte *outBuffer)
{
    pcx_t          *pcx = (pcx_t *) imgdata;
    byte           *raw = &pcx->data, *palette;
    int             x, y;
    int             dataByte, runLength;
    byte           *pix;
    short           xmax = SHORT(pcx->xmax);
    short           ymax = SHORT(pcx->ymax);

    // Check the format.
    if(pcx->manufacturer != 0x0a || pcx->version != 5 || pcx->encoding != 1 ||
       pcx->bits_per_pixel != 8)
    {
        //Con_Message("PCX_Load: unsupported format.\n");
        return NULL;
    }

    if(outBuffer)
    {
        // Check that the PCX is not larger than the buffer.
        if(xmax >= *buf_w || ymax >= *buf_h)
        {
            Con_Message("PCX_Load: larger than expected.\n");
            return NULL;
        }
    }
    else
    {
        PCX_MemoryGetSize(imgdata, buf_w, buf_h);
        outBuffer = M_Malloc(4 * *buf_w * *buf_h);
    }

    palette = M_Malloc(768);
    memcpy(palette, ((byte *) pcx) + len - 768, 768); // Palette is in the end.

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
    return outBuffer;
}

void PCX_Load(const char *fn, int buf_w, int buf_h, byte *outBuffer)
{
    PCX_AllocLoad(fn, &buf_w, &buf_h, outBuffer);
}

/**
 * PCX loader, partly borrowed from the Q2 utils source (lbmlib.c).
 */
byte *PCX_AllocLoad(const char *fn, int *buf_w, int *buf_h, byte *outBuffer)
{
    DFILE          *file = F_Open(fn, "rb");
    byte           *raw;
    size_t          len;

    if(!file)
    {
        Con_Message("PCX_Load: Can't find %s.\n", fn);
        return NULL;
    }

    // Load the file.
    F_Seek(file, 0, SEEK_END); // Seek to end.
    len = F_Tell(file); // How long?
    F_Seek(file, 0, SEEK_SET);
    raw = M_Malloc(len);
    F_Read(raw, len, file);
    F_Close(file);

    // Parse the PCX file.
    if(!(outBuffer = PCX_MemoryAllocLoad(raw, len, buf_w, buf_h, outBuffer)))
        Con_Message("PCX_Load: Error loading \"%s\".\n", M_Pretty(fn));

    M_Free(raw);
    return outBuffer;
}
