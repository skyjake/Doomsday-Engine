/**
 * @file pcx.h PCX image reader
 *
 * Originally derived from the Q2 utils source (lbmlib.c).
 *
 * @author Copyright &copy; 2006-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
 * @author Copyright &copy; 1997-2006 id Software, Inc
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBDENG_GRAPHICS_PCX_H
#define LIBDENG_GRAPHICS_PCX_H

#include "filehandle.h"

#ifdef __cplusplus
extern "C" {
#endif

/// @addtogroup resource
///@{

/**
 * Reads the given PCX image and returns a pointer to a planar
 * RGBA buffer. Width and height are set, and pixelSize is 4 (RGBA).
 * The caller must free the allocated buffer with Z_Free.
 * Width, height and pixelSize can't be NULL.
 */
uint8_t *PCX_Load(FileHandle *file, int *width, int *height, int *pixelSize);

/**
 * @return  Textual message detailing the last error encountered else @c 0.
 */
char const *PCX_LastError(void);

///@}

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_GRAPHICS_PCX_H */
