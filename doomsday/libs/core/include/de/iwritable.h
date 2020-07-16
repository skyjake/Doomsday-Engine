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

#ifndef LIBCORE_IWRITABLE_H
#define LIBCORE_IWRITABLE_H

#include "de/libcore.h"

namespace de {

class Writer;

/**
 * Interface that classes can implement if they want to support operations
 * where write-only serialization is needed. Serialization means that an
 * object is converted into a byte array so that all the relevant information
 * about the object is included. The original object can then be restored by
 * deserializing the byte array (see IReadable).
 *
 * @ingroup data
 */
class DE_PUBLIC IWritable
{
public:
    virtual ~IWritable() = default;

    /**
     * Serialize the object using the provided Writer.
     *
     * @param to  Writer using which the data is written.
     */
    virtual void operator >> (Writer &to) const = 0;
};

} // namespace de

#endif /* LIBCORE_IWRITABLE_H */
