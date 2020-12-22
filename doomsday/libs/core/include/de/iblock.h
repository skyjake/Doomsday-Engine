/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_IBLOCK_H
#define LIBCORE_IBLOCK_H

#include "de/ibytearray.h"

namespace de {

/**
 * Interface for a resizable block of memory that provides direct access
 * to the bytes.
 *
 * @ingroup data
 */
class IBlock
{
public:
    virtual ~IBlock() = default;

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
    virtual void copyFrom(const IByteArray &array, IByteArray::Offset at, IByteArray::Size count) = 0;

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
    virtual const IByteArray::Byte *data() const = 0;
};

} // namespace de

#endif // LIBCORE_IBLOCK_H
