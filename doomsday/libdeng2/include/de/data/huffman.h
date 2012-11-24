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
#include "../Block"

namespace de {
namespace codec {

/**
 * Encodes the data using Huffman codes.
 * @param data  Block of data to encode.
 *
 * @return Encoded block of bits.
 */
Block huffmanEncode(Block const &data);

/**
 * Decodes the coded message using the Huffman tree.
 * @param codedData  Block of Huffman-coded data.
 *
 * @return Decoded block of data.
 */
Block huffmanDecode(Block const &codedData);

} // namespace codec
} // namespace de

#endif // LIBDENG2_HUFFMAN_H
