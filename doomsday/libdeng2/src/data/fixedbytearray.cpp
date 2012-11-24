/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/FixedByteArray"

using namespace de;

FixedByteArray::FixedByteArray(IByteArray &mainArray) 
    : ByteSubArray(mainArray, 0, mainArray.size())
{}

FixedByteArray::FixedByteArray(IByteArray &mainArray, Offset at, Size size)
    : ByteSubArray(mainArray, at, size)
{}

FixedByteArray::FixedByteArray(IByteArray const &mainArray)
    : ByteSubArray(mainArray, 0, mainArray.size())
{}

FixedByteArray::FixedByteArray(IByteArray const &mainArray, Offset at, Size size)
    : ByteSubArray(mainArray, at, size)
{}
    
void FixedByteArray::set(Offset at, Byte const *values, Size count)
{
    // Increasing the size is not allowed.
    if(at + count > size())
    {
        throw OffsetError("FixedByteArray::set", "Fixed byte arrays cannot grow");
    }
    ByteSubArray::set(at, values, count);
}
