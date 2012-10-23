/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_IBLOCK_H
#define LIBDENG2_IBLOCK_H

#include "../IByteArray"

namespace de {

/**
 * Interface for a resizable block of memory that provides direct access
 * to the bytes.
 */
class IBlock
{
public:
    virtual ~IBlock() {}

    /// Empties the data of the block.
    virtual void clear() = 0;

    /**
     * Copies contents into the block from a byte array.
     *
     * @param array  Source data.
     * @param at     Offset within source data.
     * @param count  Number of bytes to copy. This will also be the new size
     *               of the block after the operation.
     */
    virtual void copyFrom(const IByteArray& array, IByteArray::Offset at, IByteArray::Size count) = 0;

    /**
     * Resizes the block.
     *
     * @param size  New size.
     */
    virtual void resize(IByteArray::Size size) = 0;

    /**
     * Gives const access to the data directly.
     *
     * @return Pointer to the beginning of the data.
     */
    virtual const IByteArray::Byte* data() const = 0;
};

} // namespace de

#endif // LIBDENG2_IBLOCK_H
