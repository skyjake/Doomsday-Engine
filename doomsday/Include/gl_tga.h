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
 * gl_tga.h: TGA Images
 */

#ifndef __MY_TARGA_H_
#define __MY_TARGA_H_

#include "sys_file.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef	unsigned char		uChar;		// 1 byte
typedef	unsigned short int	uShort;		// 2 bytes

// Screen origins.
#define	TGA_SCREEN_ORIGIN_LOWER	0	// Lower left-hand corner.
#define	TGA_SCREEN_ORIGIN_UPPER	1	// Upper left-hand corner.

// Data interleaving.
#define	TGA_INTERLEAVE_NONE		0	// Non-interleaved.
#define	TGA_INTERLEAVE_TWOWAY	1	// Two-way (even/odd) interleaving.
#define	TGA_INTERLEAVE_FOURWAY	2	// Four-way interleaving.

enum
{
	TGA_FALSE,
	TGA_TRUE,
	TGA_TARGA24,					// rgb888
	TGA_TARGA32						// rgba8888
};

#pragma pack(1)	// Confirm that the structures are formed correctly.
typedef struct 		// Targa image descriptor byte; a bit field
{
	uChar	attributeBits	: 4;	// Attribute bits associated with each pixel.
    uChar	reserved		: 1;	// A reserved bit; must be 0.
    uChar	screenOrigin	: 1;	// Location of screen origin; must be 0.
    uChar	dataInterleave	: 2;	// TGA_INTERLEAVE_*
} TARGA_IMAGE_DESCRIPTOR;

typedef struct
{
	uChar	idFieldSize;			// Identification field size in bytes.
	uChar	colorMapType;			// Type of the color map.
    uChar	imageType;				// Image type code.
    // Color map specification.
	uShort  colorMapOrigin;			// Index of first color map entry.
    uShort	colorMapLength;			// Number of color map entries.
	uChar	colorMapEntrySize;		// Number of bits in a color map entry (16/24/32).
    // Image specification.
    uShort	xOrigin;				// X coordinate of lower left corner.
    uShort	yOrigin;				// Y coordinate of lower left corner.
    uShort	imageWidth;				// Width of the image in pixels.
    uShort	imageHeight;			// Height of the image in pixels.
    uChar	imagePixelSize;			// Number of bits in a pixel (16/24/32).
	TARGA_IMAGE_DESCRIPTOR imageDescriptor;	// A bit field.
} TARGA_HEADER;
#pragma pack()		// Back to the default value.

// Save the rgb565 buffer as Targa 16.
int TGA_Save24_rgb565(char *filename,int w,int h,uShort *buffer);

// Save the rgb888 buffer as Targa 24.
int TGA_Save24_rgb888(char *filename, int w, int h, uChar *buffer);

int TGA_Save24_rgba8888(char *filename, int w, int h, uChar *buffer);

// Save the rgb888 buffer as Targa 16.
int TGA_Save16_rgb888(char *filename, int w, int h, uChar *buffer);

// Load an rgba8888 image (32 bits per pixel).
int TGA_Load32_rgba8888(DFILE *file, int w, int h, uChar *buffer);

// Get the dimensions of a Targa image.
int TGA_GetSize(char *filename, int *w, int *h);

#ifdef __cplusplus
}
#endif

#endif

