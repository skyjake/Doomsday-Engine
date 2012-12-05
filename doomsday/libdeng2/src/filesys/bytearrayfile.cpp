/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/ByteArrayFile"

namespace de {

IOStream &ByteArrayFile::operator << (IByteArray const &bytes)
{
    // Append the bytes to the end of the file.
    Block block(bytes);
    set(File::size(), block.data(), block.size());
    return *this;
}

IIStream &ByteArrayFile::operator >> (IByteArray &)
{
    // Bytes cannot be read from a byte array file in such a way that they would
    // be gone from the array.
    throw InputError("ByteArrayFile::operator >>", "ByteArrayFile is an immutable stream");
}

IIStream const &ByteArrayFile::operator >> (IByteArray &bytes) const
{
    // Read the entire contents of the file.
    Block block(File::size());
    get(0, block.data(), block.size());
    bytes.set(0, block.data(), block.size());
    return *this;
}

} // namespace de
