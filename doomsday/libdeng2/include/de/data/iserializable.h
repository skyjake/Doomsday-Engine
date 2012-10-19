/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 
#ifndef LIBDENG2_ISERIALIZABLE_H
#define LIBDENG2_ISERIALIZABLE_H

#include "../libdeng2.h"
#include "../IWritable"
#include "../IReadable"

namespace de {

class Writer;
class Reader;

/**
 * Interface that classes can implement if they want to support operations
 * where serialization is needed. Serialization means that an object is
 * converted into a byte array so that all the relevant information about
 * the object is included. The original object can then be restored by
 * deserializing the byte array.
 *
 * @ingroup data
 */
class DENG2_PUBLIC ISerializable : public IWritable, public IReadable
{
public:
    /// Deserialization of the provided data failed. @ingroup errors
    DENG2_ERROR(DeserializationError);

public:
    virtual ~ISerializable() {}
};

} // namespace de

#endif // LIBDENG2_ISERIALIZABLE_H
