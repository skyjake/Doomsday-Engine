/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
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

//===========================================================================
// PCX_MemoryGetSize
//	Returns true if successful.
//===========================================================================
int PCX_MemoryGetSize(void *imageData, int *w, int *h)
{
	pcx_t *hdr = (pcx_t*) imageData;

	if(hdr->manufacturer != 0x0a
		|| hdr->version != 5
		|| hdr->encoding != 1
		|| hdr->bits_per_pixel != 8) return false;		
	if(w) *w = hdr->xmax + 1;
	if(h) *h = hdr->ymax + 1;
	return true;
}

//===========================================================================
// PCX_GetSize
//===========================================================================
int PCX_GetSize(const char *fn, int *w, int *h)
{
	DFILE *file = F_Open(fn, "rb");
	pcx_t hdr;

	if(!file) return false;
	F_Read(&hdr, sizeof(hdr), file);
	F_Close(file);
	return PCX_MemoryGetSize(&hdr, w, h);
}

//===========================================================================
// PCX_MemoryLoad
//	Returns true if the data is a PCX image (probably).
//===========================================================================
int PCX_MemoryLoad(byte *imgdata, int len, int buf_w, int buf_h, 
				   byte *outBuffer)
{
	return PCX_MemoryAllocLoad(imgdata, len, &buf_w, &buf_h, outBuffer) != 0;
}

//===========================================================================
// PCX_MemoryAllocLoad
//	Returns true if the data is a PCX image (probably).
//	If outBuffer is NULL, a new buffer is allocated with M_Malloc.
//===========================================================================
byte *PCX_MemoryAllocLoad
	(byte *imgdata, int len, int *buf_w, int *buf_h, byte *outBuffer)
{
	pcx_t	*pcx = (pcx_t*) imgdata;
	byte	*raw = &pcx->data, *palette;
	int		x, y;
	int		dataByte, runLength;
	byte	*pix;

	// Check the format.
	if(pcx->manufacturer != 0x0a
		|| pcx->version != 5
		|| pcx->encoding != 1
		|| pcx->bits_per_pixel != 8)
	{
		//Con_Message("PCX_Load: unsupported format.\n");
		return NULL;
	}

	if(outBuffer)
	{
		// Check that the PCX is not larger than the buffer.
		if(pcx->xmax >= *buf_w || pcx->ymax >= *buf_h)
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

	palette = Z_Malloc(768, PU_STATIC, 0);
	memcpy(palette, ((byte*) pcx) + len - 768, 768); // Palette is in the end.

	pix = outBuffer;

	for(y = 0; y <= pcx->ymax; y++, pix += (pcx->xmax + 1)*3)
	{
		for(x = 0; x <= pcx->xmax; )
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
				memcpy(pix + x++ * 3, palette + dataByte*3, 3);
			}
		}
	}

	if(raw - (byte *)pcx > len)
		Con_Error("PCX_Load: corrupt image!\n");

	Z_Free(palette);
	return outBuffer;
}

//===========================================================================
// PCX_Load
//===========================================================================
void PCX_Load(const char *fn, int buf_w, int buf_h, byte *outBuffer)
{
	PCX_AllocLoad(fn, &buf_w, &buf_h, outBuffer);
}

//===========================================================================
// PCX_AllocLoad
//	PCX loader, partly borrowed from the Q2 utils source (lbmlib.c). 
//===========================================================================
byte *PCX_AllocLoad(const char *fn, int *buf_w, int *buf_h, byte *outBuffer)
{
	DFILE	*file = F_Open(fn, "rb");
	byte	*raw;
	int		len;

	if(!file) 
	{
		Con_Message("PCX_Load: can't find %s.\n", fn);
		return NULL;
	}

	// Load the file.
	F_Seek(file, 0, SEEK_END);		// Seek to end.
	len = F_Tell(file);			// How long?
	F_Seek(file, 0, SEEK_SET);
	raw = Z_Malloc(len, PU_STATIC, 0);
	F_Read(raw, len, file);
	F_Close(file);

	// Parse the PCX file.
	if(!(outBuffer = PCX_MemoryAllocLoad(raw, len, buf_w, buf_h, outBuffer)))
		Con_Message("PCX_Load: error loading %s.\n", fn);

	Z_Free(raw);
	return outBuffer;
}

