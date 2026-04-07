/**
 * @file huffman.h
 * Huffman codes. @ingroup data
 *
 * @see CLR 2nd Ed, p. 388
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2009-2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#ifndef LIBCORE_HUFFMAN_H
#define LIBCORE_HUFFMAN_H

#include "de/libcore.h"
#include "de/block.h"

namespace de {
namespace codec {

/**
 * Encodes the data using Huffman codes.
 * @param data  Block of data to encode.
 *
 * @return Encoded block of bits.
 */
Block huffmanEncode(const Block &data);

/**
 * Decodes the coded message using the Huffman tree.
 * @param codedData  Block of Huffman-coded data.
 *
 * @return Decoded block of data.
 */
Block huffmanDecode(const Block &codedData);

} // namespace codec
} // namespace de

#endif // LIBCORE_HUFFMAN_H
