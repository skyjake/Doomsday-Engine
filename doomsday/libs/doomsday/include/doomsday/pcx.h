/** @file pcx.h  PCX image reader.
 *
 * Originally derived from the Q2 utils source (lbmlib.c).
 *
 * @authors Copyright © 2006-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1997-2006 id Software, Inc
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

#ifndef LIBDOOMSDAY_RESOURCE_PCX_H
#define LIBDOOMSDAY_RESOURCE_PCX_H

#ifdef __cplusplus

#include <doomsday/filesys/filehandle.h>
#include <de/vector.h>

/// @addtogroup resource
///@{

/**
 * Reads the given PCX image and returns a pointer to a planar RGBA buffer.
 * The caller must free the allocated buffer with Z_Free.
 */
LIBDOOMSDAY_PUBLIC uint8_t *PCX_Load(res::FileHandle &file, de::Vec2ui &outSize, int &pixelSize);

/**
 * @return  Textual message detailing the last error encountered else @c 0.
 */
LIBDOOMSDAY_PUBLIC const char *PCX_LastError();

///@}

#endif //__cplusplus

#endif // LIBDOOMSDAY_RESOURCE_PCX_H
