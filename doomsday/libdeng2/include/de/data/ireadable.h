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
 
#ifndef LIBDENG2_IREADABLE_H
#define LIBDENG2_IREADABLE_H

#include "../libdeng2.h"

namespace de {

class Reader;

/**
 * Interface that classes can implement if they want to support operations
 * where read-only deserialization is needed. Deserialization means that an
 * object is restored from its serialized version in a byte array
 * (see IWritable).
 *
 * @ingroup data
 */
class DENG2_PUBLIC IReadable
{
public:
    virtual ~IReadable() {}

    /**
     * Restore the object from the provided Reader.
     *
     * @param from  Reader where the data is read from.
     */
    virtual void operator << (Reader& from) = 0;
};

} // namespace de

#endif // LIBDENG2_IREADABLE_H
