/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/fixedbytearray.h"

namespace de {

FixedByteArray::FixedByteArray(IByteArray &mainArray) 
    : ByteSubArray(mainArray, 0, mainArray.size())
{}

FixedByteArray::FixedByteArray(IByteArray &mainArray, Offset at, Size size)
    : ByteSubArray(mainArray, at, size)
{}

FixedByteArray::FixedByteArray(const IByteArray &mainArray)
    : ByteSubArray(mainArray, 0, mainArray.size())
{}

FixedByteArray::FixedByteArray(const IByteArray &mainArray, Offset at, Size size)
    : ByteSubArray(mainArray, at, size)
{}
    
void FixedByteArray::set(Offset at, const Byte *values, Size count)
{
    // Increasing the size is not allowed.
    if (at + count > size())
    {
        throw OffsetError("FixedByteArray::set", "Fixed byte arrays cannot grow");
    }
    ByteSubArray::set(at, values, count);
}

} // namespace de