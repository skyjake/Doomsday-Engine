/** @file tga.h  Truevision TGA (a.k.a Targa) image reader
 *
 * @authors Copyright © 2003-2019 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2009-2013 Daniel Swanson <danij@dengine.net>
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

#pragma once

#ifdef __cplusplus

#include <doomsday/filesys/filehandle.h>
#include <de/Vector>

/**
 * Loads a 24-bit or a 32-bit image (24-bit color + 8-bit alpha).
 *
 * @warning: This is not a generic TGA loader. Only type 2, 24/32 pixel
 *     size, attrbits 0/8 and lower left origin supported.
 *
 * @return  Non-zero iff the image is loaded successfully.
 */
uint8_t *TGA_Load(de::FileHandle &file, de::Vector2ui &outSize, int &pixelSize);

#endif // __cplusplus
