/** @file m_decomp64.h  DOOM64 decompression algorithm.
 *
 * Used with various lumps of DOOM64 data.
 *
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBDOOMSDAY_MISC_DECOMPRESS64_H
#define LIBDOOMSDAY_MISC_DECOMPRESS64_H

#include "libdoomsday.h"
#include <de/legacy/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * DOOM64 data decompression algorithm.
 *
 * \todo Needs further analysis and documentation.
 *       Get rid of the fixed-size working buffer used with byte sequences.
 *       Clean up
 *
 * @param dst           Output buffer. Must be large enough!
 * @param src           Src buffer (the compressed data).
 */
LIBDOOMSDAY_PUBLIC void M_Decompress64(byte* dst, const byte* src);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDOOMSDAY_MISC_DECOMPRESS64_H
