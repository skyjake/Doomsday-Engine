/**\file gl_tga.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2009-2012 Daniel Swanson <danij@dengine.net>
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
 * Truevision TGA (a.k.a Targa) image reader/writer.
 */

#ifndef LIBDENG_GRAPHICS_TGA_H
#define LIBDENG_GRAPHICS_TGA_H

#include "dfile.h"

/**
 * Saves the buffer (which is formatted rgb565) to a Targa 24 image file.
 *
 * @param file          Handle to the open file to be written to.
 * @param w             Width of the image in pixels.
 * @param h             Height of the image in pixels.
 * @param buf           Ptr to the image data to be written.
 *
 * @return  Non-zero iff successful.
 */
int TGA_Save24_rgb565(FILE* file, int w, int h, const uint16_t* buffer);

/**
 * Save the rgb888 buffer as Targa 24.
 *
 * @param file          Handle to the open file to be written to.
 * @param w             Width of the image in pixels.
 * @param h             Height of the image in pixels.
 * @param buf           Ptr to the image data to be written.
 *
 * @return  Non-zero iff successful.
 */
int TGA_Save24_rgb888(FILE* file, int w, int h, const uint8_t* buf);

/**
 * Save the rgb8888 buffer as Targa 24.
 *
 * @param file          Handle to the open file to be written to.
 * @param w             Width of the image in pixels.
 * @param h             Height of the image in pixels.
 * @param buf           Ptr to the image data to be written.
 *
 * @return  Non-zero iff successful.
 */
int TGA_Save24_rgba8888(FILE* file, int w, int h, const uint8_t* buf);

/**
 * Save the rgb888 buffer as Targa 16.
 *
 * @param file          Handle to the open file to be written to.
 * @param w             Width of the image in pixels.
 * @param h             Height of the image in pixels.
 * @param buf           Ptr to the image data to be written.
 *
 * @return  Non-zero iff successful.
 */
int TGA_Save16_rgb888(FILE* file, int w, int h, const uint8_t* buf);

/**
 * Loads a 24-bit or a 32-bit image (24-bit color + 8-bit alpha).
 *
 * \warning: This is not a generic TGA loader. Only type 2, 24/32 pixel
 *     size, attrbits 0/8 and lower left origin supported.
 *
 * @return  Non-zero iff the image is loaded successfully.
 */
uint8_t* TGA_Load(DFile* file, int* width, int* height, int* pixelSize);

/**
 * @return  Textual message detailing the last error encountered else @c 0.
 */
const char* TGA_LastError(void);

#endif /* LIBDENG_GRAPHICS_TGA_H */
