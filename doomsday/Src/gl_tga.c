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
 * gl_tga.c: TGA Images
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "de_system.h"
#include "de_graphics.h"

// Saves the buffer (which is formatted rgb565) to a Targa 24 image file.
int TGA_Save24_rgb565(char *filename,int w,int h,uShort *buffer)
{
	FILE 			*file;
    TARGA_HEADER	header;
    int				i,k;
	uChar			*saveBuf;
	int				saveBufStart = w*h-1;	// From the end.

    if((file = fopen(filename,"wb")) == NULL) return 0;

	saveBuf = malloc(w * h * 3);
	
    // Now we have the file. Let's setup the Targa header.
	memset(&header, 0, sizeof(header));
	header.idFieldSize = 0;			// No identification field.
    header.colorMapType = 0;		// No color map.
    header.imageType = 2;			// Targa type 2 (unmapped RGB).
    header.xOrigin = 0;
    header.yOrigin = 0;
    header.imageWidth = w;
    header.imageHeight = h;
	header.imagePixelSize = 24;
	header.imageDescriptor.attributeBits = 0;
    header.imageDescriptor.reserved = 0;		// Must be zero.
    header.imageDescriptor.screenOrigin = TGA_SCREEN_ORIGIN_LOWER;	// Must be.
    header.imageDescriptor.dataInterleave = TGA_INTERLEAVE_NONE;

    // Write the header.
    fwrite(&header,sizeof(header),1,file);

    // Then comes the actual image data. We'll first make a copy of the buffer
    // and convert the pixel data format.
    //uShort *saveBuf = new uShort[w*h];
    
	// Let's start to convert the buffer.
    for(k=0; k<h; k++)
		for(i=0; i<w; i++)	// pixels are at (i,k)
	    {
     		// Our job is to convert:
	        // 	buffer: 	RRRRRGGGGGGBBBBB 	rgb565
          	// Into:
     	    // 	saveBuf:	ARRRRRGGGGGBBBBB	argb1555 (a=0)
			uShort r,g,b;
          	uShort src = buffer[k*w+i];
			int destIndex = (saveBufStart-((k+1)*w-1-i))*3;

     	    r = (src>>11) & 0x1f;		// The last 5 bits.
			g = (src>>5) & 0x3f;		// The middle 6 bits (one bit'll be lost).
     	    b = src & 0x1f;			// The first 5 bits.
          // Let's put the result in the destination.
//               int newcol = (r<<10)+(g<<5)+b;
             // Let's flip the bytes around.
//               newcol = ((newcol>>8)&0xff) + ((newcol&0xff)<<8);
            
			saveBuf[destIndex+2] = b<<3;
            saveBuf[destIndex] = g<<2;
            saveBuf[destIndex+1] = r<<3;
            //saveBuf[saveBufStart-((k+1)*w-1-i)] = newcol;
     }
	 /// Write the converted buffer (bytes may go the wrong way around...!).
     fwrite(saveBuf, w*h*3, 1, file);
     // Clean up.
//     delete [] saveBuf;
	 free(saveBuf);
     fclose(file);
     // A successful saving operation.
     return 1;
}

// Save the rgb888 buffer as Targa 24.
int TGA_Save24_rgb888(char *filename, int w, int h, uChar *buffer)
{
	FILE 			*file;
    TARGA_HEADER	header;
    int				i;
	uChar			*savebuf;

    if((file=fopen(filename, "wb")) == NULL) return 0; // Huh?

	savebuf = malloc(w*h*3);

    // Now we have the file. Let's setup the Targa header.
	memset(&header, 0, sizeof(header));
	header.idFieldSize = 0;			// No identification field.
    header.colorMapType = 0;		// No color map.
    header.imageType = 2;			// Targa type 2 (unmapped RGB).
    header.xOrigin = 0;
    header.yOrigin = 0;
    header.imageWidth = w;
    header.imageHeight = h;
	header.imagePixelSize = 24;		// 8 bits per channel.
	header.imageDescriptor.attributeBits = 0;
    header.imageDescriptor.reserved = 0;		// Must be zero.
    header.imageDescriptor.screenOrigin = TGA_SCREEN_ORIGIN_LOWER;	// Must be.
    header.imageDescriptor.dataInterleave = TGA_INTERLEAVE_NONE;

    // Write the header.
    fwrite(&header, sizeof(header), 1, file);

	// The save format is BRG.
	for(i=0; i<w*h; i++)
	{
		uChar *ptr = buffer + i*3, *save = savebuf + i*3;
		save[0] = ptr[2];	
		save[1] = ptr[1];	
		save[2] = ptr[0];	
	}
	fwrite(savebuf, w*h*3, 1, file);

	free(savebuf);
	fclose(file);
	// A success!
	return 1;
}

int TGA_Save24_rgba8888(char *filename, int w, int h, uChar *buffer)
{
	FILE 			*file;
    TARGA_HEADER	header;
    int				i;
	uChar			*savebuf;

    if((file=fopen(filename, "wb")) == NULL) return 0; // Huh?

	savebuf = malloc(w*h*3);

    // Now we have the file. Let's setup the Targa header.
	memset(&header, 0, sizeof(header));
	header.idFieldSize = 0;			// No identification field.
    header.colorMapType = 0;		// No color map.
    header.imageType = 2;			// Targa type 2 (unmapped RGB).
    header.xOrigin = 0;
    header.yOrigin = 0;
    header.imageWidth = w;
    header.imageHeight = h;
	header.imagePixelSize = 24;		// 8 bits per channel.
	header.imageDescriptor.attributeBits = 0;
    header.imageDescriptor.reserved = 0;		// Must be zero.
    header.imageDescriptor.screenOrigin = TGA_SCREEN_ORIGIN_LOWER;	// Must be.
    header.imageDescriptor.dataInterleave = TGA_INTERLEAVE_NONE;

    // Write the header.
    fwrite(&header, sizeof(header), 1, file);

	// The save format is BGR.
	for(i=0; i<w*h; i++)
	{
		uChar *ptr = buffer + i*4, *save = savebuf + i*3;
		save[0] = ptr[2];	
		save[1] = ptr[1];	
		save[2] = ptr[0];	
	}
	fwrite(savebuf, w*h*3, 1, file);

	free(savebuf);
	fclose(file);
	// A success!
	return 1;
}

// Save the rgb888 buffer as Targa 16.
int TGA_Save16_rgb888(char *filename, int w, int h, uChar *buffer)
{
	FILE 			*file;
    TARGA_HEADER	header;
    int				i;
	uShort			*savebuf;

    if((file=fopen(filename, "wb")) == NULL) return 0; // Huh?

	savebuf = malloc(w*h*2);

    // Now we have the file. Let's setup the Targa header.
	memset(&header, 0, sizeof(header));
	header.idFieldSize = 0;			// No identification field.
    header.colorMapType = 0;		// No color map.
    header.imageType = 2;			// Targa type 2 (unmapped RGB).
    header.xOrigin = 0;
    header.yOrigin = 0;
    header.imageWidth = w;
    header.imageHeight = h;
	header.imagePixelSize = 16;		// 8 bits per channel.
	header.imageDescriptor.attributeBits = 0;
    header.imageDescriptor.reserved = 0;		// Must be zero.
    header.imageDescriptor.screenOrigin = TGA_SCREEN_ORIGIN_LOWER;	// Must be.
    header.imageDescriptor.dataInterleave = TGA_INTERLEAVE_NONE;

    // Write the header.
    fwrite(&header, sizeof(header), 1, file);

	// The save format is BRG.
	for(i=0; i<w*h; i++)
	{
		uChar *ptr = buffer + i*3;
		// The format is _RRRRRGG GGGBBBBB.
		savebuf[i] = (ptr[2]>>3) + ((ptr[1]&0xf8)<<2) + ((ptr[0]&0xf8)<<7);
	}
	fwrite(savebuf, w*h*2, 1, file);

	free(savebuf);
	fclose(file);
	// A success!
	return 1;
}

//===========================================================================
// TGA_Load32_rgba8888
//	Loads a 24-bit or a 32-bit TGA image (24-bit color + 8-bit alpha). 
//	Caller must allocate enough memory for 'buffer' (at least 4*w*h). 
//	Returns non-zero iff the image is loaded successfully. 
//
//	Warning: This is not a generic TGA loader. Only type 2, 24/32 pixel 
//		size, attrbits 0/8 and lower left origin supported.
//===========================================================================
int TGA_Load32_rgba8888(DFILE *file, int w, int h, uChar *buffer)
{
    TARGA_HEADER	header;
    int				x, y, pixbytes, format;
	uChar			*out, *inbuf, *in;

	// Read and check the header.
	F_Read(&header, sizeof(header), file);
	if(header.imageType != 2
		|| (header.imagePixelSize != 32 && header.imagePixelSize != 24)
		|| (header.imageDescriptor.attributeBits != 8
			&& header.imageDescriptor.attributeBits != 0)
		|| header.imageDescriptor.screenOrigin != TGA_SCREEN_ORIGIN_LOWER)
	{
		// May or may not get displayed...
		printf("loadTGA32_rgba8888: I don't know this format!\n");
		printf("  (type=%i pxsize=%i abits=%i)\n", header.imageType,
			header.imagePixelSize, header.imageDescriptor.attributeBits);
		return TGA_FALSE;
	}

	printf("(TGA: type=%i pxsize=%i abits=%i)\n", header.imageType,
		header.imagePixelSize, header.imageDescriptor.attributeBits);

	// Determine format.
	if(header.imagePixelSize == 24)
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
	inbuf = malloc(w * h * pixbytes);
	F_Read(inbuf, w * h * pixbytes, file);

	// "Unpack" the pixels (origin in the lower left corner).
	in = inbuf;
	for(y = h-1; y >= 0; y--)
		for(x = 0; x < w; x++)
		{
			out = buffer + (y*w + x)*pixbytes;
			// TGA pixels are in BGRA format.
			out[2] = *in++;
			out[1] = *in++;
			out[0] = *in++;
			if(pixbytes == 4) out[3] = *in++;
		}
	free(inbuf);

	// Success!
	return format;
}

//===========================================================================
// TGA_GetSize
//	Returns true if the file was found and successfully read.
//===========================================================================
int TGA_GetSize(char *filename, int *w, int *h)
{
    TARGA_HEADER header;
	DFILE *file = F_Open(filename, "rb");

	if(!file) return 0;
	F_Read(&header, sizeof(header), file);
	F_Close(file);
	if(w) *w = header.imageWidth;
	if(h) *h = header.imageHeight;
	return 1;
}
