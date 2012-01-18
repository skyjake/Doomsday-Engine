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

#ifndef LIBDENG_HUFFMAN_H
#define LIBDENG_HUFFMAN_H

/**
 * Builds the Huffman tree and initializes the code lookup.
 */
void Huff_Init(void);

/**
 * Free all resources allocated for Huffman codes.
 */
void Huff_Shutdown(void);

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
void* Huff_Encode(const byte *data, size_t size, size_t *encodedSize);

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
byte* Huff_Decode(const void *data, size_t size, size_t *decodedSize);

#endif // LIBDENG_HUFFMAN_H
