/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBDENG2_BLOCK_H
#define LIBDENG2_BLOCK_H

#include "../IByteArray"
#include "../IBlock"

#include <QByteArray>

namespace de {

/**
 * Simple data buffer that implements the byte array interface.
 *
 * @ingroup data
 */
class DENG2_PUBLIC Block : public QByteArray, public IByteArray, public IBlock
{
public:
    Block(Size initialSize = 0);
    Block(const IByteArray& array);
    Block(const Block& other);
    Block(const QByteArray& byteArray);

    /**
     * Construct a new block and copy its contents from the specified
     * location at another array.
     *
     * @param array  Source data.
     * @param at     Offset within source data.
     * @param count  Number of bytes to copy. Also the size of the new block.
     */
    Block(const IByteArray& array, Offset at, Size count);

    // Implements IByteArray.
    Size size() const;
    void get(Offset at, Byte* values, Size count) const;
    void set(Offset at, const Byte* values, Size count);

    // Implements IBlock.
    void clear();
    void copyFrom(const IByteArray& array, Offset at, Size count);
    void resize(Size size);
    const Byte* data() const;

    Byte* data();

    /// Appends a block after this one.
    Block& operator += (const Block& other);

    /// Copies the contents of another block.
    Block& operator = (const Block& other);
};

} // namespace de
    
#endif // LIBDENG2_BLOCK_H
