/**
 * @file iostream.h
 * Output-only stream interface. @ingroup data
 *
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBDENG2_IOSTREAM_H
#define LIBDENG2_IOSTREAM_H

#include "../IByteArray"

namespace de {

/**
 * Interface that allows communication via an output stream of bytes.
 */
class IOStream
{
public:
    /// Output to the stream failed. @ingroup errors
    DENG2_ERROR(OutputError);

public:
    virtual ~IOStream() {}

    /**
     * Writes an array of bytes to the stream.
     *
     * @param bytes  Byte array to write.
     *
     * @return Reference to this IOStream object.
     */
    virtual IOStream& operator << (const IByteArray& bytes) = 0;
};

} // namespace de

#endif // LIBDENG2_IOSTREAM_H
