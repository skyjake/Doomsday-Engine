/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * gl_tga.h: TGA Images
 */

#ifndef __TARGA_FILE_FORMAT_LIB__
#define __TARGA_FILE_FORMAT_LIB__

#include "sys_file.h"

#ifdef __cplusplus
extern          "C" {
#endif

enum {
    TGA_FALSE,
    TGA_TRUE,
    TGA_TARGA24, // rgb888
    TGA_TARGA32 // rgba8888
};

// Save the rgb565 buffer as Targa 16.
int             TGA_Save24_rgb565(const char* filename, int w, int h,
                                  const ushort* buffer);

// Save the rgb888 buffer as Targa 24.
int             TGA_Save24_rgb888(const char* filename, int w, int h,
                                  const byte* buf);

int             TGA_Save24_rgba8888(const char* filename, int w, int h,
                                    const byte* buf);

// Save the rgb888 buffer as Targa 16.
int             TGA_Save16_rgb888(const char* filename, int w, int h,
                                  const byte* buf);

// Load an rgba8888 image (32 bits per pixel).
int             TGA_Load32_rgba8888(DFILE* file, int w, int h,
                                    byte* buf);

// Get the dimensions of a Targa image.
int             TGA_GetSize(const char* filename, int* w, int* h);

#ifdef __cplusplus
}
#endif
#endif
