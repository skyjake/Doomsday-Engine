/**\file gl_png.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2011 Daniel Swanson <danij@dengine.net>
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
 * PNG (Portable Network Graphics) image reader.
 *
 * Requires libpng and zlib.
 */

#ifndef LIBDENG_GRAPHICS_PNG_H
#define LIBDENG_GRAPHICS_PNG_H

struct DFILE;

/**
 * Reads the given PNG image and returns a pointer to a planar RGB or
 * RGBA buffer. Width and height are set, and pixelSize is either 3 (RGB)
 * or 4 (RGBA). The caller must free the allocated buffer with Z_Free.
 * Width, height and pixelSize can't be NULL. Handles 1-4 channels.
 */
uint8_t* PNG_Load(DFILE* file, int* width, int* height, int* pixelSize);

/**
 * @return  Textual message detailing the last error encountered else @c 0.
 */
const char* PNG_LastError(void);

#endif /* LIBDENG_GRAPHICS_PNG_H */
