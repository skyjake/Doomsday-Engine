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
 * gl_pcx.h: PCX Images
 */

#ifndef __DOOMSDAY_GRAPHICS_PCX_H__
#define __DOOMSDAY_GRAPHICS_PCX_H__

#pragma pack(1)
typedef struct
{
    char			manufacturer;
    char			version;
    char			encoding;
    char			bits_per_pixel;
    unsigned short	xmin,ymin,xmax,ymax;
    unsigned short	hres,vres;
    unsigned char	palette[48];
    char			reserved;
    char			color_planes;
    unsigned short	bytes_per_line;
    unsigned short	palette_type;
    char			filler[58];
    unsigned char	data;			// unbounded
} pcx_t;
#pragma pack()

int		PCX_MemoryGetSize(void *imageData, int *w, int *h);
int		PCX_GetSize(const char *fn, int *w, int *h);
int		PCX_MemoryLoad(byte *imgdata, int len, int buf_w, int buf_h, byte *outBuffer);
byte*	PCX_MemoryAllocLoad(byte *imgdata, int len, int *buf_w, int *buf_h, byte *outBuffer);
void	PCX_Load(const char *fn, int buf_w, int buf_h, byte *outBuffer);
byte*	PCX_AllocLoad(const char *fn, int *buf_w, int *buf_h, byte *outBuffer);

#endif 
