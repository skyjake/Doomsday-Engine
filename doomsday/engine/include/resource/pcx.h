/**\file pcx.h
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
 * PCX image reader.
 *
 * Partly borrowed from the Q2 utils source (lbmlib.c).
 */

#ifndef LIBDENG_GRAPHICS_PCX_H
#define LIBDENG_GRAPHICS_PCX_H

#include "filehandle.h"

/**
 * Reads the given PCX image and returns a pointer to a planar
 * RGBA buffer. Width and height are set, and pixelSize is 4 (RGBA).
 * The caller must free the allocated buffer with Z_Free.
 * Width, height and pixelSize can't be NULL.
 */
uint8_t* PCX_Load(FileHandle* file, int* width, int* height, int* pixelSize);

/**
 * @return  Textual message detailing the last error encountered else @c 0.
 */
const char* PCX_LastError(void);

#endif /* LIBDENG_GRAPHICS_PCX_H */
