/**
 * @file huffman.h
 * Huffman codes. @ingroup math
 *
 * @see CLR 2nd Ed, p. 388
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2009-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG2_HUFFMAN_H
#define LIBDENG2_HUFFMAN_H

#include "../libdeng2.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Encodes the data using Huffman codes.
 *
 * @param data         Block of data to encode.
 * @param size         Size of data to encode.
 * @param encodedSize  The number of bytes in the encoded data is written here.
 *
 * @return Pointer to the encoded block of bits. Caller gets ownership and must
 * delete the data with M_Free().
 */
void* DENG2_PUBLIC Huffman_Encode(const dbyte* data, dsize size, dsize *encodedSize);

/**
 * Decodes the coded message using the Huffman tree.
 *
 * @param data         Block of Huffman-coded data.
 * @param size         Size of the data to decode.
 * @param decodedSize  The number of bytes in the decoded data is written here.
 *
 * @return Pointer to the decoded data. Caller gets ownership and must delete
 * the data with M_Free().
 */
dbyte* DENG2_PUBLIC Huffman_Decode(const void *data, dsize size, dsize *decodedSize);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDENG2_HUFFMAN_H
