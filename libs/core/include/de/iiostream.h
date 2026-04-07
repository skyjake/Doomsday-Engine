/**
 * @file iiostream.h
 * I/O stream interface.
 *
 * @authors Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_IIOSTREAM_H
#define LIBCORE_IIOSTREAM_H

#include "de/iistream.h"
#include "de/iostream.h"

namespace de {

/**
 * Interface that allows communication via an input/output stream of bytes. @ingroup data
 * @see IIStream, IOStream
 */
class IIOStream : public IIStream, public IOStream
{
public:
    /// Only reading is allowed from the stream. @ingroup errors
    DE_SUB_ERROR(OutputError, ReadOnlyError);
};

} // namespace de

#endif // LIBCORE_IIOSTREAM_H
