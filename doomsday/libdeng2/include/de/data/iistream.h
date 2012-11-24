/**
 * @file iistream.h
 * Input-only stream interface.
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

#ifndef LIBDENG2_IISTREAM_H
#define LIBDENG2_IISTREAM_H

#include "../IByteArray"

namespace de {

/**
 * Interface that allows communication via an input stream of bytes.
 *
 * When reading from the stream, it can either be in modifiable (non-const) or
 * immutable (const) mode. When reading bytes from a modifiable stream, the
 * bytes are then removed from the stream and the next read will return a
 * different set of bytes. Immutable streams, on the other hand, can be used
 * for peeking ahead into the received data: the bytes are not removed from the
 * stream during the read.
 */
class IIStream
{
public:
    /// Input from the stream failed. @ingroup errors
    DENG2_ERROR(InputError);

public:
    virtual ~IIStream() {}

    /**
     * Reads all the available bytes into the array @a bytes. If there is
     * nothing available for reading, nothing is written to @a bytes. The
     * stream operates as a modifiable object: once read, the bytes are
     * considered to be removed from the stream.
     *
     * @param bytes  Read bytes are stored here.
     *
     * @return Reference to this IIStream object.
     */
    virtual IIStream &operator >> (IByteArray &bytes) = 0;

    /**
     * Reads all the available bytes into the array @a bytes. If there is
     * nothing available for reading, nothing is written to @a bytes.
     *
     * Immutable streams can be used for peeking ahead into the received data.
     * Here the stream operates as a const object: the read bytes are not
     * removed from the stream, and a subsequent read will return the same
     * bytes, plus any additional bytes that may have been produced in the
     * meantime.
     *
     * @param bytes  Read bytes are stored here.
     *
     * @return Reference to this IIStream object.
     */
    virtual IIStream const &operator >> (IByteArray &bytes) const = 0;
};

} // namespace de

#endif // LIBDENG2_IISTREAM_H
