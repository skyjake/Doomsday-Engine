/**
 * @file iiostream.h
 * I/O stream interface.
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

#ifndef LIBDENG2_IIOSTREAM_H
#define LIBDENG2_IIOSTREAM_H

#include "../IIStream"
#include "../IOStream"

namespace de {

/**
 * Interface that allows communication via an input/output stream of bytes.
 * @see IIStream, IOStream
 */
class IIOStream : public IIStream, public IOStream
{
public:
    /// Only reading is allowed from the stream. @ingroup errors
    DENG2_SUB_ERROR(OutputError, ReadOnlyError);
};

} // namespace de

#endif // LIBDENG2_IIOSTREAM_H
